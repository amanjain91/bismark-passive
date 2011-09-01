#include "flow_table.h"
#include "packet_series.h"

#include <stdio.h>
#include <stdlib.h>

#include <check.h>

/********************************************************
 * Packet series tests
 ********************************************************/

static packet_series_t series;
static const int kMySize = 12;
static const int kMyFlowId = 1;
static const uint32_t kMySec = 100;
static const uint32_t kMyUSec = 20000;

void series_setup () {
  packet_series_init (&series);
}

START_TEST (test_series_add) {
  struct timeval first_tv, second_tv;

  first_tv.tv_sec = kMySec;
  first_tv.tv_usec = kMyUSec;
  fail_if (packet_series_add_packet (&series, &first_tv, kMySize, kMyFlowId));
  fail_unless (series.length == 1);
  fail_unless (series.start_time.tv_sec == kMySec);
  fail_unless (series.start_time.tv_usec == kMyUSec);
  fail_if (series.discarded_by_overflow);
  fail_unless (series.packet_data[0].timestamp == 0);
  fail_unless (series.packet_data[0].size == kMySize);
  fail_unless (series.packet_data[0].flow == kMyFlowId);

  second_tv.tv_sec = kMySec * 2;
  second_tv.tv_usec = kMyUSec * 2;
  fail_if (packet_series_add_packet (&series, &second_tv, kMySize * 2, kMyFlowId));
  fail_unless (series.length == 2);
  fail_unless (series.start_time.tv_sec == first_tv.tv_sec);
  fail_unless (series.start_time.tv_usec == first_tv.tv_usec);
  fail_if (series.discarded_by_overflow);
  fail_unless (series.packet_data[1].timestamp
      == ((second_tv.tv_sec * NUM_MICROS_PER_SECOND + second_tv.tv_usec)
          - (kMySec * NUM_MICROS_PER_SECOND + kMyUSec)));
  fail_unless (series.packet_data[1].size == kMySize * 2);
  fail_unless (series.packet_data[1].flow == kMyFlowId);
}
END_TEST

START_TEST (test_series_overflow) {
  int idx;
  struct timeval tv;

  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;

  for (idx = 0; idx < PACKET_DATA_BUFFER_ENTRIES; ++idx) {
    fail_if (packet_series_add_packet (&series, &tv, kMySize, kMyFlowId));
    fail_unless (series.length == idx + 1);
    fail_unless (series.start_time.tv_sec == kMySec);
    fail_unless (series.start_time.tv_usec == kMyUSec);
    fail_if (series.discarded_by_overflow);
  }

  for (idx = 0; idx < 10; ++idx) {
    fail_unless (packet_series_add_packet (&series, &tv, kMySize, kMyFlowId));
    fail_unless (series.length == PACKET_DATA_BUFFER_ENTRIES);
    fail_unless (series.start_time.tv_sec == kMySec);
    fail_unless (series.start_time.tv_usec == kMyUSec);
    fail_unless (series.discarded_by_overflow == idx + 1);
  }
}
END_TEST

/********************************************************
 * Flow table tests
 ********************************************************/

static flow_table_t table;

static uint32_t dummy_hash(const char* data, int len) {
  return 0;
}

void flows_setup() {
  flow_table_init(&table);
  testing_set_hash_function(&dummy_hash);
}

START_TEST(test_flows_detect_dupes) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;

  struct timeval tv;
  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;

  fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.num_elements == 1);
  fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.num_elements == 1);
}
END_TEST

START_TEST(test_flows_can_probe) {
  struct timeval tv;
  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;

  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_unless(flow_table_process_flow(&table, &entry, &tv) == 0);
  fail_unless(table.entries[0].occupied == ENTRY_OCCUPIED);
  fail_unless(table.num_elements == 1);

  entry.ip_source = 10;
  fail_unless(flow_table_process_flow(&table, &entry, &tv) == 1);
  fail_unless(table.entries[1].occupied == ENTRY_OCCUPIED);
  fail_unless(table.num_elements == 2);

  entry.ip_source = 20;
  fail_unless(flow_table_process_flow(&table, &entry, &tv) == 3);
  fail_unless(table.entries[3].occupied == ENTRY_OCCUPIED);
  fail_unless(table.num_elements == 3);

  int num_adds = table.num_elements;
  while (num_adds < HT_NUM_PROBES) {
    entry.ip_source = num_adds * 10;
    fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
    fail_unless(table.num_elements == num_adds);
    ++num_adds;
  }

  entry.ip_source = 11;
  fail_unless(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.num_elements == HT_NUM_PROBES);
  fail_unless(table.num_dropped_flows == 1);
  fail_unless(table.num_expired_flows == 0);
}
END_TEST

START_TEST(test_flows_can_set_base_timestamp) {
  struct timeval tv;
  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.base_timestamp_seconds == tv.tv_sec);

  struct timeval next_tv;
  next_tv.tv_sec = tv.tv_sec + 1;
  next_tv.tv_usec = kMyUSec;
  entry.ip_source = 10;
  fail_if(flow_table_process_flow(&table, &entry, &next_tv) < 0);
  fail_unless(table.base_timestamp_seconds == tv.tv_sec);

  next_tv.tv_sec = next_tv.tv_sec + FLOW_TABLE_EXPIRATION_SECONDS + 1;
  next_tv.tv_usec = kMyUSec;
  fail_if(flow_table_process_flow(&table, &entry, &next_tv) < 0);
  fail_unless(table.base_timestamp_seconds == next_tv.tv_sec);

  fail_unless(table.num_expired_flows == 2);
  fail_unless(table.num_dropped_flows == 0);
}
END_TEST

START_TEST(test_flows_can_set_last_update_time) {
  struct timeval tv;
  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;

  struct timeval next_tv;
  next_tv.tv_sec = kMySec * 2;
  next_tv.tv_usec = kMyUSec * 2;

  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_unless(flow_table_process_flow(&table, &entry, &tv) == 0);
  fail_unless(table.entries[0].last_update_time_seconds == 0);
  fail_unless(table.num_elements == 1);

  fail_unless(flow_table_process_flow(&table, &entry, &next_tv) == 0);
  fail_unless(table.entries[0].last_update_time_seconds == (next_tv.tv_sec - tv.tv_sec));
  fail_unless(table.num_elements == 1);

  fail_unless(table.num_expired_flows == 0);
  fail_unless(table.num_dropped_flows == 0);
}
END_TEST

START_TEST(test_flows_can_expire) {
  struct timeval tv;
  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.num_elements == 1);

  entry.ip_source = 2;
  fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.num_elements == 2);

  struct timeval next_tv;
  next_tv.tv_sec = kMySec + FLOW_TABLE_EXPIRATION_SECONDS + 1;
  next_tv.tv_usec = kMyUSec;
  entry.ip_source = 3;
  fail_unless(flow_table_process_flow(&table, &entry, &next_tv) == 0);
  fail_unless(table.num_elements == 1);
  fail_unless(table.entries[0].occupied == ENTRY_OCCUPIED);
  fail_unless(table.entries[1].occupied == ENTRY_DELETED);
  fail_unless(table.num_expired_flows == 2);
  fail_unless(table.num_dropped_flows == 0);
}
END_TEST

START_TEST(test_flows_can_detect_later_dupes) {
  struct timeval tv;
  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, &tv) < 0);
  fail_unless(table.num_elements == 1);

  struct timeval next_tv;
  next_tv.tv_sec = tv.tv_sec + 1;
  next_tv.tv_usec = tv.tv_usec;
  entry.ip_source = 2;
  fail_if(flow_table_process_flow(&table, &entry, &next_tv) < 0);
  fail_unless(table.num_elements == 2);

  next_tv.tv_sec = tv.tv_sec + FLOW_TABLE_EXPIRATION_SECONDS + 1;
  fail_unless(flow_table_process_flow(&table, &entry, &next_tv) == 1);
  fail_unless(table.num_elements == 1);
  fail_unless(table.entries[0].occupied == ENTRY_DELETED);
  fail_unless(table.entries[1].occupied == ENTRY_OCCUPIED);
  fail_unless(table.num_expired_flows == 1);
}
END_TEST

Suite* build_suite () {
  Suite *s = suite_create("Bismark passive");

  TCase *tc_series = tcase_create("Packet series");
  tcase_add_checked_fixture(tc_series, series_setup, NULL);
  tcase_add_test(tc_series, test_series_add);
  tcase_add_test(tc_series, test_series_overflow);
  suite_add_tcase(s, tc_series);

  TCase *tc_flows = tcase_create("Flow table");
  tcase_add_checked_fixture(tc_flows, flows_setup, NULL);
  tcase_add_test(tc_flows, test_flows_detect_dupes);
  tcase_add_test(tc_flows, test_flows_can_probe);
  tcase_add_test(tc_flows, test_flows_can_set_base_timestamp);
  tcase_add_test(tc_flows, test_flows_can_set_last_update_time);
  tcase_add_test(tc_flows, test_flows_can_expire);
  tcase_add_test(tc_flows, test_flows_can_detect_later_dupes);
  suite_add_tcase(s, tc_flows);

  return s;
}

int main (int argc, char* argv[]) {
  int number_failed;
  Suite *s = build_suite ();
  SRunner *sr = srunner_create (s);
  srunner_run_all (sr, CK_NORMAL);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}