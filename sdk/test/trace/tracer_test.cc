#include "opentelemetry/sdk/trace/tracer.h"
#include "opentelemetry/exporters/memory/in_memory_span_exporter.h"
#include "opentelemetry/sdk/trace/samplers/always_off.h"
#include "opentelemetry/sdk/trace/samplers/always_on.h"
#include "opentelemetry/sdk/trace/samplers/parent_or_else.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/span_data.h"

#include <gtest/gtest.h>

using namespace opentelemetry::sdk::trace;
using opentelemetry::core::SteadyTimestamp;
using opentelemetry::core::SystemTimestamp;
namespace nostd  = opentelemetry::nostd;
namespace common = opentelemetry::common;
using opentelemetry::exporter::memory::InMemorySpanData;
using opentelemetry::exporter::memory::InMemorySpanExporter;
using opentelemetry::trace::SpanContext;

/**
 * A mock sampler that returns non-empty sampling results attributes.
 */
class MockSampler final : public Sampler
{
public:
  SamplingResult ShouldSample(const SpanContext * /*parent_context*/,
                              trace_api::TraceId /*trace_id*/,
                              nostd::string_view /*name*/,
                              trace_api::SpanKind /*span_kind*/,
                              const trace_api::KeyValueIterable & /*attributes*/) noexcept override
  {
    // Return two pairs of attributes. These attributes should be added to the
    // span attributes
    return {Decision::RECORD_AND_SAMPLE,
            nostd::unique_ptr<const std::map<std::string, opentelemetry::common::AttributeValue>>(
                new const std::map<std::string, opentelemetry::common::AttributeValue>(
                    {{"sampling_attr1", 123}, {"sampling_attr2", "string"}}))};
  }

  nostd::string_view GetDescription() const noexcept override { return "MockSampler"; }
};

namespace
{
std::shared_ptr<opentelemetry::trace::Tracer> initTracer(
    std::unique_ptr<InMemorySpanExporter> &&exporter)
{
  auto processor = std::make_shared<SimpleSpanProcessor>(std::move(exporter));
  return std::shared_ptr<opentelemetry::trace::Tracer>(new Tracer(processor));
}

std::shared_ptr<opentelemetry::trace::Tracer> initTracer(
    std::unique_ptr<InMemorySpanExporter> &&exporter,
    std::shared_ptr<Sampler> sampler)
{
  auto processor = std::make_shared<SimpleSpanProcessor>(std::move(exporter));
  return std::shared_ptr<opentelemetry::trace::Tracer>(new Tracer(processor, sampler));
}
}  // namespace

TEST(Tracer, ToInMemorySpanExporter)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));

  auto span_first  = tracer->StartSpan("span 1");
  auto scope_first = tracer->WithActiveSpan(span_first);
  auto span_second = tracer->StartSpan("span 2");

  ASSERT_EQ(0, span_data->GetSpans().size());

  span_second->End();

  auto span2 = span_data->GetSpans();
  ASSERT_EQ(1, span2.size());
  ASSERT_EQ("span 2", span2.at(0)->GetName());
  EXPECT_TRUE(span2.at(0)->GetTraceId().IsValid());
  EXPECT_TRUE(span2.at(0)->GetSpanId().IsValid());
  EXPECT_TRUE(span2.at(0)->GetParentSpanId().IsValid());

  span_first->End();

  auto span1 = span_data->GetSpans();
  ASSERT_EQ(1, span1.size());
  ASSERT_EQ("span 1", span1.at(0)->GetName());
  EXPECT_TRUE(span1.at(0)->GetTraceId().IsValid());
  EXPECT_TRUE(span1.at(0)->GetSpanId().IsValid());
  EXPECT_FALSE(span1.at(0)->GetParentSpanId().IsValid());

  // Verify trace and parent span id propagation
  EXPECT_EQ(span1.at(0)->GetTraceId(), span2.at(0)->GetTraceId());
  EXPECT_EQ(span2.at(0)->GetParentSpanId(), span1.at(0)->GetSpanId());
}

TEST(Tracer, StartSpanSampleOn)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_on                              = initTracer(std::move(exporter));

  tracer_on->StartSpan("span 1")->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_LT(std::chrono::nanoseconds(0), cur_span_data->GetStartTime().time_since_epoch());
  ASSERT_LT(std::chrono::nanoseconds(0), cur_span_data->GetDuration());
}

TEST(Tracer, StartSpanSampleOff)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_off = initTracer(std::move(exporter), std::make_shared<AlwaysOffSampler>());

  // This span will not be recorded.
  tracer_off->StartSpan("span 2")->End();

  // The span doesn't write any span data because the sampling decision is alway
  // DROP.
  ASSERT_EQ(0, span_data->GetSpans().size());
}

TEST(Tracer, StartSpanWithOptionsTime)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));

  opentelemetry::trace::StartSpanOptions start;
  start.start_system_time = SystemTimestamp(std::chrono::nanoseconds(300));
  start.start_steady_time = SteadyTimestamp(std::chrono::nanoseconds(10));

  opentelemetry::trace::EndSpanOptions end;
  end.end_steady_time = SteadyTimestamp(std::chrono::nanoseconds(40));

  tracer->StartSpan("span 1", start)->End(end);

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(std::chrono::nanoseconds(300), cur_span_data->GetStartTime().time_since_epoch());
  ASSERT_EQ(std::chrono::nanoseconds(30), cur_span_data->GetDuration());
}

TEST(Tracer, StartSpanWithAttributes)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));

  // Start a span with all supported scalar attribute types.
  tracer
      ->StartSpan("span 1", {{"attr1", "string"},
                             {"attr2", false},
                             {"attr1", 314159},
                             {"attr3", (unsigned int)314159},
                             {"attr4", (int32_t)-20},
                             {"attr5", (uint32_t)20},
                             {"attr6", (int64_t)-20},
                             {"attr7", (uint64_t)20},
                             {"attr8", 3.1},
                             {"attr9", "string"}})
      ->End();

  // Start a span with all supported array attribute types.
  int listInt[]                       = {1, 2, 3};
  unsigned int listUInt[]             = {1, 2, 3};
  int32_t listInt32[]                 = {1, -2, 3};
  uint32_t listUInt32[]               = {1, 2, 3};
  int64_t listInt64[]                 = {1, -2, 3};
  uint64_t listUInt64[]               = {1, 2, 3};
  double listDouble[]                 = {1.1, 2.1, 3.1};
  bool listBool[]                     = {true, false};
  nostd::string_view listStringView[] = {"a", "b"};
  std::map<std::string, common::AttributeValue> m;
  m["attr1"] = nostd::span<int>(listInt);
  m["attr2"] = nostd::span<unsigned int>(listUInt);
  m["attr3"] = nostd::span<int32_t>(listInt32);
  m["attr4"] = nostd::span<uint32_t>(listUInt32);
  m["attr5"] = nostd::span<int64_t>(listInt64);
  m["attr6"] = nostd::span<uint64_t>(listUInt64);
  m["attr7"] = nostd::span<double>(listDouble);
  m["attr8"] = nostd::span<bool>(listBool);
  m["attr9"] = nostd::span<nostd::string_view>(listStringView);

  tracer->StartSpan("span 2", m)->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(2, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(9, cur_span_data->GetAttributes().size());
  ASSERT_EQ(314159, nostd::get<int32_t>(cur_span_data->GetAttributes().at("attr1")));
  ASSERT_EQ(false, nostd::get<bool>(cur_span_data->GetAttributes().at("attr2")));
  ASSERT_EQ(314159, nostd::get<uint32_t>(cur_span_data->GetAttributes().at("attr3")));
  ASSERT_EQ(-20, nostd::get<int32_t>(cur_span_data->GetAttributes().at("attr4")));
  ASSERT_EQ(20, nostd::get<uint32_t>(cur_span_data->GetAttributes().at("attr5")));
  ASSERT_EQ(-20, nostd::get<int64_t>(cur_span_data->GetAttributes().at("attr6")));
  ASSERT_EQ(20, nostd::get<uint64_t>(cur_span_data->GetAttributes().at("attr7")));
  ASSERT_EQ(3.1, nostd::get<double>(cur_span_data->GetAttributes().at("attr8")));
  ASSERT_EQ("string", nostd::get<std::string>(cur_span_data->GetAttributes().at("attr9")));

  auto &cur_span_data2 = spans.at(1);
  ASSERT_EQ(9, cur_span_data2->GetAttributes().size());
  ASSERT_EQ(std::vector<int32_t>({1, 2, 3}),
            nostd::get<std::vector<int32_t>>(cur_span_data2->GetAttributes().at("attr1")));
  ASSERT_EQ(std::vector<uint32_t>({1, 2, 3}),
            nostd::get<std::vector<uint32_t>>(cur_span_data2->GetAttributes().at("attr2")));
  ASSERT_EQ(std::vector<int32_t>({1, -2, 3}),
            nostd::get<std::vector<int32_t>>(cur_span_data2->GetAttributes().at("attr3")));
  ASSERT_EQ(std::vector<uint32_t>({1, 2, 3}),
            nostd::get<std::vector<uint32_t>>(cur_span_data2->GetAttributes().at("attr4")));
  ASSERT_EQ(std::vector<int64_t>({1, -2, 3}),
            nostd::get<std::vector<int64_t>>(cur_span_data2->GetAttributes().at("attr5")));
  ASSERT_EQ(std::vector<uint64_t>({1, 2, 3}),
            nostd::get<std::vector<uint64_t>>(cur_span_data2->GetAttributes().at("attr6")));
  ASSERT_EQ(std::vector<double>({1.1, 2.1, 3.1}),
            nostd::get<std::vector<double>>(cur_span_data2->GetAttributes().at("attr7")));
  ASSERT_EQ(std::vector<bool>({true, false}),
            nostd::get<std::vector<bool>>(cur_span_data2->GetAttributes().at("attr8")));
  ASSERT_EQ(std::vector<std::string>({"a", "b"}),
            nostd::get<std::vector<std::string>>(cur_span_data2->GetAttributes().at("attr9")));
}

TEST(Tracer, StartSpanWithAttributesCopy)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));

  {
    std::unique_ptr<std::vector<int64_t>> numbers(new std::vector<int64_t>);
    numbers->push_back(1);
    numbers->push_back(2);
    numbers->push_back(3);

    std::unique_ptr<std::vector<nostd::string_view>> strings(new std::vector<nostd::string_view>);
    std::string s1("a");
    std::string s2("b");
    std::string s3("c");
    strings->push_back(s1);
    strings->push_back(s2);
    strings->push_back(s3);
    tracer
        ->StartSpan("span 1",
                    {{"attr1", *numbers}, {"attr2", nostd::span<nostd::string_view>(*strings)}})
        ->End();
  }

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(2, cur_span_data->GetAttributes().size());

  auto numbers = nostd::get<std::vector<int64_t>>(cur_span_data->GetAttributes().at("attr1"));
  ASSERT_EQ(3, numbers.size());
  ASSERT_EQ(1, numbers[0]);
  ASSERT_EQ(2, numbers[1]);
  ASSERT_EQ(3, numbers[2]);

  auto strings = nostd::get<std::vector<std::string>>(cur_span_data->GetAttributes().at("attr2"));
  ASSERT_EQ(3, strings.size());
  ASSERT_EQ("a", strings[0]);
  ASSERT_EQ("b", strings[1]);
  ASSERT_EQ("c", strings[2]);
}

TEST(Tracer, GetSampler)
{
  // Create a Tracer with a default AlwaysOnSampler
  std::shared_ptr<SpanProcessor> processor_1(new SimpleSpanProcessor(nullptr));
  std::shared_ptr<Tracer> tracer_on(new Tracer(std::move(processor_1)));

  auto t1 = tracer_on->GetSampler();
  ASSERT_EQ("AlwaysOnSampler", t1->GetDescription());

  // Create a Tracer with a AlwaysOffSampler
  std::shared_ptr<SpanProcessor> processor_2(new SimpleSpanProcessor(nullptr));
  std::shared_ptr<Tracer> tracer_off(
      new Tracer(std::move(processor_2), std::make_shared<AlwaysOffSampler>()));

  auto t2 = tracer_off->GetSampler();
  ASSERT_EQ("AlwaysOffSampler", t2->GetDescription());
}

TEST(Tracer, SpanSetAttribute)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));

  auto span = tracer->StartSpan("span 1");

  span->SetAttribute("abc", 3.1);

  span->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());
  auto &cur_span_data = spans.at(0);
  ASSERT_EQ(3.1, nostd::get<double>(cur_span_data->GetAttributes().at("abc")));
}

TEST(Tracer, SpanSetEvents)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));

  auto span = tracer->StartSpan("span 1");
  span->AddEvent("event 1");
  span->AddEvent("event 2", std::chrono::system_clock::now());
  span->AddEvent("event 3", std::chrono::system_clock::now(), {{"attr1", 1}});
  span->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());

  auto &span_data_events = spans.at(0)->GetEvents();
  ASSERT_EQ(3, span_data_events.size());
  ASSERT_EQ("event 1", span_data_events[0].GetName());
  ASSERT_EQ("event 2", span_data_events[1].GetName());
  ASSERT_EQ("event 3", span_data_events[2].GetName());
  ASSERT_EQ(0, span_data_events[0].GetAttributes().size());
  ASSERT_EQ(0, span_data_events[1].GetAttributes().size());
  ASSERT_EQ(1, span_data_events[2].GetAttributes().size());
}

TEST(Tracer, TestAlwaysOnSampler)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_on                              = initTracer(std::move(exporter));

  // Testing AlwaysOn sampler.
  // Create two spans for each tracer. Check the exported result.
  auto span_on_1 = tracer_on->StartSpan("span 1");
  auto span_on_2 = tracer_on->StartSpan("span 2");
  span_on_2->End();
  span_on_1->End();

  auto spans = span_data->GetSpans();
  ASSERT_EQ(2, spans.size());
  ASSERT_EQ("span 2", spans.at(0)->GetName());  // span 2 ends first.
  ASSERT_EQ("span 1", spans.at(1)->GetName());
}

TEST(Tracer, TestAlwaysOffSampler)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer_off = initTracer(std::move(exporter), std::make_shared<AlwaysOffSampler>());
  auto span_off_1 = tracer_off->StartSpan("span 1");
  auto span_off_2 = tracer_off->StartSpan("span 2");

  span_off_1->SetAttribute("attr1", 3.1);  // Not recorded.

  span_off_2->End();
  span_off_1->End();

  // The tracer export nothing with an AlwaysOff sampler
  ASSERT_EQ(0, span_data->GetSpans().size());
}

TEST(Tracer, TestParentOrElseSampler)
{
  // Current ShouldSample always pass an empty ParentContext,
  // so this sampler will work as an AlwaysOnSampler.
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data_parent_on = exporter->GetData();
  auto tracer_parent_on =
      initTracer(std::move(exporter),
                 std::make_shared<ParentOrElseSampler>(std::make_shared<AlwaysOnSampler>()));

  auto span_parent_on_1 = tracer_parent_on->StartSpan("span 1");
  auto span_parent_on_2 = tracer_parent_on->StartSpan("span 2");

  span_parent_on_1->SetAttribute("attr1", 3.1);

  span_parent_on_2->End();
  span_parent_on_1->End();

  auto spans = span_data_parent_on->GetSpans();
  ASSERT_EQ(2, spans.size());
  ASSERT_EQ("span 2", spans.at(0)->GetName());
  ASSERT_EQ("span 1", spans.at(1)->GetName());

  // Current ShouldSample always pass an empty ParentContext,
  // so this sampler will work as an AlwaysOnSampler.
  std::unique_ptr<InMemorySpanExporter> exporter2(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data_parent_off = exporter2->GetData();
  auto tracer_parent_off =
      initTracer(std::move(exporter2),
                 std::make_shared<ParentOrElseSampler>(std::make_shared<AlwaysOffSampler>()));

  auto span_parent_off_1 = tracer_parent_off->StartSpan("span 1");
  auto span_parent_off_2 = tracer_parent_off->StartSpan("span 2");

  span_parent_off_1->SetAttribute("attr1", 3.1);

  span_parent_off_1->End();
  span_parent_off_2->End();
  ASSERT_EQ(0, span_data_parent_off->GetSpans().size());
}

TEST(Tracer, WithActiveSpan)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));
  auto spans                                  = span_data.get()->GetSpans();

  ASSERT_EQ(0, spans.size());

  {
    auto span_first  = tracer->StartSpan("span 1");
    auto scope_first = tracer->WithActiveSpan(span_first);

    {
      auto span_second  = tracer->StartSpan("span 2");
      auto scope_second = tracer->WithActiveSpan(span_second);

      spans = span_data->GetSpans();
      ASSERT_EQ(0, spans.size());

      span_second->End();
    }

    spans = span_data->GetSpans();
    ASSERT_EQ(1, spans.size());
    EXPECT_EQ("span 2", spans.at(0)->GetName());

    span_first->End();
  }

  spans = span_data->GetSpans();
  ASSERT_EQ(1, spans.size());
  EXPECT_EQ("span 1", spans.at(0)->GetName());
}

TEST(Tracer, ExpectParent)
{
  std::unique_ptr<InMemorySpanExporter> exporter(new InMemorySpanExporter());
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  auto tracer                                 = initTracer(std::move(exporter));
  auto spans                                  = span_data.get()->GetSpans();

  ASSERT_EQ(0, spans.size());

  auto span_first = tracer->StartSpan("span 1");

  trace_api::StartSpanOptions options;
  options.parent   = span_first->GetContext();
  auto span_second = tracer->StartSpan("span 2", options);

  options.parent  = span_second->GetContext();
  auto span_third = tracer->StartSpan("span 3", options);

  span_third->End();
  span_second->End();
  span_first->End();

  spans = span_data->GetSpans();
  ASSERT_EQ(3, spans.size());
  auto spandata_first  = std::move(spans.at(2));
  auto spandata_second = std::move(spans.at(1));
  auto spandata_third  = std::move(spans.at(0));
  EXPECT_EQ("span 1", spandata_first->GetName());
  EXPECT_EQ("span 2", spandata_second->GetName());
  EXPECT_EQ("span 3", spandata_third->GetName());

  EXPECT_EQ(spandata_first->GetSpanId(), spandata_second->GetParentSpanId());
  EXPECT_EQ(spandata_second->GetSpanId(), spandata_third->GetParentSpanId());
}
