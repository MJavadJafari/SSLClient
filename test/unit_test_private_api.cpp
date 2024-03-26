#define log_d(...); printf(__VA_ARGS__); printf("\n");
#define log_i(...); printf(__VA_ARGS__); printf("\n");
#define log_w(...); printf(__VA_ARGS__); printf("\n");
#define log_e(...); printf(__VA_ARGS__); printf("\n");
#define log_v(...); printf(__VA_ARGS__); printf("\n");
#define portTICK_PERIOD_MS 1
#define vTaskDelay(x) delay(x)

#include "unity.h"
#include "Arduino.h"
#include "mocks/ESPClass.hpp"
#include "mocks/TestClient.h"
#include "ssl_client.cpp"

using namespace fakeit;

TestClient testClient;
sslclient_context *testContext;

void setUp(void) {
  ArduinoFakeReset();
  testClient.reset();
  testClient.returns("connected", (uint8_t)1);
  mbedtls_mock_reset_return_values();
  testContext = new sslclient_context();
}

void tearDown(void) {
  delete testContext;
  testContext = nullptr;
}

/* Test client_net_send function */

void test_client_null_context(void) {
  // Arrange
  unsigned char buf[100];
  
  // Act
  int result = SSLCLIENT_client_net_send(NULL, buf, sizeof(buf));
  
  // Assert
  TEST_ASSERT_EQUAL_INT(-1, result);
} 
    
void test_client_write_succeeds(void) {
  // Arrange
  testClient.returns("write", (size_t)1024).then((size_t)1024).then((size_t)1024);
  unsigned char buf[3072]; // 3 chunks of data

  // Act
  void* clientPtr = static_cast<void*>(&testClient);
  int result = SSLCLIENT_client_net_send(clientPtr, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(3072, result);
}

void test_client_write_fails(void) {
  // Arrange
  testClient.returns("write", (size_t)1024).then((size_t)1024).then((size_t)0);
  unsigned char buf[3000]; // 3 chunks of data, but it fails on the 3rd chunk

  // Act
  int result = SSLCLIENT_client_net_send(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(MBEDTLS_ERR_NET_SEND_FAILED, result);
}

void test_zero_length_buffer(void) {
  // Arrange
  unsigned char buf[1];

  // Act
  int result = SSLCLIENT_client_net_send(&testClient, buf, 0);
  
  // Assert
  TEST_ASSERT_EQUAL_INT(0, result);
}

void test_single_chunk_exact(void) {
  // Arrange
  unsigned char buf[1024];
  testClient.returns("write", (size_t)1024);

  // Act
  int result = SSLCLIENT_client_net_send(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(1024, result);
}

void test_partial_write(void) {
  // Arrange
  unsigned char buf[3000];
  testClient.returns("write", (size_t)500).then((size_t)500).then((size_t)500);

  // Act
  int result = SSLCLIENT_client_net_send(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(1500, result); // Only half the buffer is sent
}

void test_disconnected_client(void) {
  // Arrange
  unsigned char buf[1000];
  testClient.reset(); // Reset the mock client
  testClient.returns("connected", (uint8_t)0); // Mock the client to return false for "connected" function

  // Act
  int result = SSLCLIENT_client_net_send(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(-2, result); // -2 indicates disconnected client
}

void run_client_net_send_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_client_null_context);
  RUN_TEST(test_client_write_succeeds);
  RUN_TEST(test_client_write_fails);
  RUN_TEST(test_zero_length_buffer);
  RUN_TEST(test_single_chunk_exact);
  RUN_TEST(test_partial_write);
  RUN_TEST(test_disconnected_client);
  UNITY_END();
}

/* Test get_ssl_receive function */

void test_get_ssl_receive_success(void) {
  // Arrange
  unsigned char data[1024];
  mbedtls_ssl_read_returns = 1024;

  // Act
  int result = get_ssl_receive(testContext, data, sizeof(data));

  // Assert
  TEST_ASSERT_EQUAL_INT(1024, result);
}

void test_get_ssl_receive_partial_read(void) {
  // Arrange
  unsigned char data[1024];
  mbedtls_ssl_read_returns = 512;

  // Act
  int result = get_ssl_receive(testContext, data, sizeof(data));

  // Assert
  TEST_ASSERT_EQUAL_INT(512, result);
}

void test_get_ssl_receive_failure(void) {
  // Arrange
  unsigned char data[1024];
  mbedtls_ssl_read_returns = MBEDTLS_ERR_SSL_BAD_INPUT_DATA;

  // Act
  int result = get_ssl_receive(testContext, data, sizeof(data));

  // Assert
  TEST_ASSERT_EQUAL_INT(MBEDTLS_ERR_SSL_BAD_INPUT_DATA, result);
}

void test_get_ssl_receive_zero_length(void) {
  // Arrange
  unsigned char data[1];
  mbedtls_ssl_read_returns = 0;

  // Act
  int result = get_ssl_receive(testContext, data, 0);

  // Assert
  TEST_ASSERT_EQUAL_INT(0, result);
}

void run_get_ssl_receive_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_get_ssl_receive_success);
  RUN_TEST(test_get_ssl_receive_partial_read);
  RUN_TEST(test_get_ssl_receive_failure);
  RUN_TEST(test_get_ssl_receive_zero_length);
  UNITY_END();
}

/* Test client_net_recv function */

void test_null_client_context(void) {
  // Arrange
  unsigned char buf[100];

  // Act
  int result = SSLCLIENT_client_net_recv(NULL, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_disconnected_client_client_net_recv(void) {
  // Arrange
  testClient.reset();
  testClient.returns("connected", (uint8_t)0);
  unsigned char buf[100];

  // Act
  int result = SSLCLIENT_client_net_recv(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(-2, result);
}

void test_successful_client_read(void) {
  // Arrange
  unsigned char buf[100];
  testClient.returns("read", (int)50);

  // Act
  int result = SSLCLIENT_client_net_recv(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(50, result);
}

void test_failed_client_read(void) {
  // Arrange
  unsigned char buf[100];
  testClient.returns("read", (int)0); // Mock a read failure

  // Act
  int result = SSLCLIENT_client_net_recv(&testClient, buf, sizeof(buf));

  // Assert
  TEST_ASSERT_EQUAL_INT(0, result); // Expecting 0 as read() has failed
}

void run_client_net_recv_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_null_client_context);
  RUN_TEST(test_disconnected_client_client_net_recv);
  RUN_TEST(test_successful_client_read);
  RUN_TEST(test_failed_client_read);
  UNITY_END();
}

/* End of test functions */

#ifdef ARDUINO

#include <Arduino.h>

void setup() {
  delay(2000); // If using Serial, allow time for serial monitor to open
  run_all_tests();
}

void loop() {
  // Empty loop
}

#else

int main(int argc, char **argv) {
  run_client_net_send_tests();
  run_get_ssl_receive_tests();
  run_client_net_recv_tests();
  return 0;
}

#endif
