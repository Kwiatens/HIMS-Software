#include <unity.h>

#include "HimsScanCore.h"
#include "HimsScanOutbox.h"

using namespace hims_scan;

void test_quantity_composer_defaults_and_limit() {
  QuantityComposer composer;
  TEST_ASSERT_EQUAL_STRING("1", composer.displayText().c_str());
  TEST_ASSERT_EQUAL_INT(1, composer.valueOrOne());

  composer.appendDigit('0');
  composer.appendDigit('2');
  composer.appendDigit('5');
  composer.appendDigit('7');
  composer.appendDigit('9');
  TEST_ASSERT_EQUAL_STRING("0257", composer.displayText().c_str());
  TEST_ASSERT_EQUAL_INT(257, composer.valueOrOne());

  TEST_ASSERT_EQUAL_INT(257, composer.consume(true));
  TEST_ASSERT_TRUE(composer.empty());
}

void test_quantity_composer_subtract_and_invalid_digits() {
  QuantityComposer composer;
  composer.appendDigit('A');
  composer.appendDigit('#');
  composer.appendDigit('3');
  TEST_ASSERT_EQUAL_STRING("3", composer.displayText().c_str());
  TEST_ASSERT_EQUAL_INT(-3, composer.consume(false));
  TEST_ASSERT_TRUE(composer.empty());
}

void test_request_json_serialization() {
  const auto json = buildQuantityRequestJson("  R1  ", "id-123", "HIMS-001", -4);
  TEST_ASSERT_EQUAL_STRING(
      "{\"deviceId\":\"R1\",\"requestId\":\"id-123\",\"code\":\"HIMS-001\",\"delta\":-4}",
      json.c_str());
}

void test_request_id_format() {
  const auto requestId = makeRequestId("  HIMS-SCAN-R1  ", 42);
  TEST_ASSERT_EQUAL_STRING("HIMS-SCAN-R1-42", requestId.c_str());
}

void test_fixed_ring_buffer_fifo_and_overwrite() {
  FixedRingBuffer<int, 3> buffer;
  TEST_ASSERT_TRUE(buffer.push(1));
  TEST_ASSERT_TRUE(buffer.push(2));
  TEST_ASSERT_TRUE(buffer.push(3));
  TEST_ASSERT_TRUE(buffer.full());

  TEST_ASSERT_TRUE(buffer.push(4));

  int value = 0;
  TEST_ASSERT_TRUE(buffer.pop(value));
  TEST_ASSERT_EQUAL_INT(2, value);
  TEST_ASSERT_TRUE(buffer.pop(value));
  TEST_ASSERT_EQUAL_INT(3, value);
  TEST_ASSERT_TRUE(buffer.pop(value));
  TEST_ASSERT_EQUAL_INT(4, value);
  TEST_ASSERT_TRUE(buffer.empty());
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_quantity_composer_defaults_and_limit);
  RUN_TEST(test_quantity_composer_subtract_and_invalid_digits);
  RUN_TEST(test_request_json_serialization);
  RUN_TEST(test_request_id_format);
  RUN_TEST(test_fixed_ring_buffer_fifo_and_overwrite);
  UNITY_END();
}

void loop() {}
