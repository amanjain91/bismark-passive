#include "dns_parser.h"
#include "device_throughput_table.h"
#include "dns_table.h"
#include "flow_table.h"
#include "address_table.h"
#include "packet_series.h"
#include "util.h"
#include "whitelist.h"

#ifdef _BLOOM_WHITELIST_H_
#include "bloom-whitelist.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <check.h>

/********************************************************
 * Utility functions
 ********************************************************/
static char temp_filename[255];
static gzFile open_tempfile() {
  char template[] = "/tmp/bismark-passive-test.XXXXXX.gz";
  int fd = mkstemps(template, 3);
  fail_if(fd < 0);
  strcpy(temp_filename, template);
  gzFile handle = gzdopen(fd, "wb");
  fail_if(handle == NULL);
  return handle;
}

static char* read_tempfile(gzFile whandle, int* len) {
  fail_if(gzclose(whandle));

  gzFile handle = gzopen(temp_filename, "rb");
  fail_if(handle == NULL);

  int capacity = 1024;
  char* contents = malloc(capacity);
  fail_unless(contents != NULL);
  int bytes_read;
  *len = 0;
  while ((bytes_read = gzread(handle, contents + *len, 1024)) > 0) {
    *len += bytes_read;
    if (*len + 1024 > capacity) {
      capacity += 1024;
      contents = realloc(contents, capacity);
      fail_unless(contents != NULL);
    }
  }
  gzclose(handle);
  return contents;
}

/********************************************************
 * Packet series tests
 ********************************************************/
static packet_series_t series;
static const int kMySize = 12;
static const int kMyFlowId = 1;
static const time_t kMySec = 123456789;
static const uint32_t kMyUSec = 20000;

void series_setup() {
  packet_series_init(&series);
}

START_TEST(test_series_add) {
  struct timeval first_tv, second_tv;

  first_tv.tv_sec = kMySec;
  first_tv.tv_usec = kMyUSec;
  fail_if(packet_series_add_packet(&series, &first_tv, kMySize, kMyFlowId) < 0);
  fail_unless(series.length == 1);
  fail_unless(series.start_time_microseconds == kMySec * NUM_MICROS_PER_SECOND + kMyUSec);
  fail_unless(series.last_time_microseconds == series.start_time_microseconds);
  fail_if(series.discarded_by_overflow);
  fail_unless(series.packet_data[0].timestamp == 0);
  fail_unless(series.packet_data[0].size == kMySize);
  fail_unless(series.packet_data[0].flow == kMyFlowId);

  second_tv.tv_sec = kMySec + 60;
  second_tv.tv_usec = kMyUSec + 1000;
  fail_if(packet_series_add_packet(&series, &second_tv, kMySize * 2, kMyFlowId) < 0);
  fail_unless(series.length == 2);
  fail_unless(series.start_time_microseconds == TIMEVAL_TO_MICROS(&first_tv));
  fail_unless(series.last_time_microseconds == TIMEVAL_TO_MICROS(&second_tv));
  fail_if(series.discarded_by_overflow);
  fail_unless(series.packet_data[1].timestamp
    == TIMEVAL_TO_MICROS(&second_tv) - TIMEVAL_TO_MICROS(&first_tv));
  fail_unless(series.packet_data[1].size == kMySize * 2);
  fail_unless(series.packet_data[1].flow == kMyFlowId);
}
END_TEST

START_TEST(test_series_overflow) {
  int idx;
  struct timeval tv;

  tv.tv_sec = kMySec;
  tv.tv_usec = kMyUSec;

  for (idx = 0; idx < PACKET_DATA_BUFFER_ENTRIES; ++idx) {
    fail_if(packet_series_add_packet(&series, &tv, kMySize, kMyFlowId) < 0);
    fail_unless(series.length == idx + 1);
    fail_unless(series.start_time_microseconds
        == kMySec * NUM_MICROS_PER_SECOND + kMyUSec);
    fail_if(series.discarded_by_overflow);
  }

  for (idx = 0; idx < 10; ++idx) {
    fail_unless(packet_series_add_packet(&series, &tv, kMySize, kMyFlowId) < 0);
    fail_unless(series.length == PACKET_DATA_BUFFER_ENTRIES);
    fail_unless(series.start_time_microseconds
        == kMySec * NUM_MICROS_PER_SECOND + kMyUSec);
    fail_unless(series.discarded_by_overflow == idx + 1);
  }
}
END_TEST

START_TEST(test_series_write_update) {
  struct timeval tv;
  tv.tv_sec = 123456789;
  tv.tv_usec = 4321;
  fail_if(packet_series_add_packet(&series, &tv, 25, 1) < 0);
  tv.tv_sec = 123456790;
  tv.tv_usec = 4321;
  fail_if(packet_series_add_packet(&series, &tv, 1024, 2) < 0);

  gzFile handle = open_tempfile();
  fail_if(packet_series_write_update(&series, handle));

  int len;
  char* contents = read_tempfile(handle, &len);
  char* expected_contents = \
      "123456789004321 0\n"
      "0 25 1\n"
      "1000000 1024 2\n"
      "\n";
  fail_if(memcmp(contents, expected_contents, len));
  free(contents);
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

void flows_simulate_update() {
  int idx;
  for (idx = 0; idx < FLOW_TABLE_ENTRIES; ++idx) {
    if (table.entries[idx].occupied == ENTRY_OCCUPIED_BUT_UNSENT) {
      table.entries[idx].occupied = ENTRY_OCCUPIED;
    }
  }
}

START_TEST(test_flows_detect_dupes) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;

  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 1);
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 1);
}
END_TEST

START_TEST(test_flows_can_probe) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_unless(flow_table_process_flow(&table, &entry, kMySec)
      == FLOW_ID_FIRST_UNRESERVED);
  fail_unless(table.entries[0].occupied == ENTRY_OCCUPIED_BUT_UNSENT);
  fail_unless(table.num_elements == 1);

  entry.ip_source = 10;
  fail_unless(flow_table_process_flow(&table, &entry, kMySec)
      == FLOW_ID_FIRST_UNRESERVED + 1);
  fail_unless(table.entries[1].occupied == ENTRY_OCCUPIED_BUT_UNSENT);
  fail_unless(table.num_elements == 2);

  entry.ip_source = 20;
  fail_unless(flow_table_process_flow(&table, &entry, kMySec)
      == FLOW_ID_FIRST_UNRESERVED + 3);
  fail_unless(table.entries[3].occupied == ENTRY_OCCUPIED_BUT_UNSENT);
  fail_unless(table.num_elements == 3);

  int num_adds = table.num_elements;
  while (num_adds < HT_NUM_PROBES) {
    entry.ip_source = num_adds * 10;
    fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
    fail_unless(table.num_elements == num_adds);
    ++num_adds;
  }

  entry.ip_source = 11;
  fail_unless(flow_table_process_flow(&table, &entry, kMySec) == FLOW_ID_ERROR);
  fail_unless(table.num_elements == HT_NUM_PROBES);
  fail_unless(table.num_dropped_flows == 1);
  fail_unless(table.num_expired_flows == 0);
}
END_TEST

START_TEST(test_flows_can_set_base_timestamp) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.base_timestamp_seconds == kMySec);

  entry.ip_source = 10;
  fail_if(flow_table_process_flow(&table, &entry, kMySec + 1) < 0);
  fail_unless(table.base_timestamp_seconds == kMySec);

  flows_simulate_update();

  time_t new_timestamp = kMySec + 1 + FLOW_TABLE_EXPIRATION_SECONDS + 1;
  fail_if(flow_table_process_flow(&table, &entry, new_timestamp) < 0);
  fail_unless(table.base_timestamp_seconds == new_timestamp);

  fail_unless(table.num_expired_flows == 2);
  fail_unless(table.num_dropped_flows == 0);
}
END_TEST

START_TEST(test_flows_can_advance_base_timestamp) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 1);

  flow_table_advance_base_timestamp(&table, kMySec + 1);
  fail_unless(table.num_elements == 1);
  int idx;
  for (idx = 0; idx < FLOW_TABLE_ENTRIES; ++idx) {
    if (table.entries[idx].occupied == ENTRY_OCCUPIED_BUT_UNSENT
        || table.entries[idx].occupied == ENTRY_OCCUPIED) {
      fail_unless(table.entries[idx].last_update_time_seconds == -1);
    }
  }

  flow_table_advance_base_timestamp(&table,
                                    kMySec - FLOW_TABLE_MIN_UPDATE_OFFSET + 1);
  fail_unless(table.num_elements == 0);
  for (idx = 0; idx < FLOW_TABLE_ENTRIES; ++idx) {
    fail_if(table.entries[idx].occupied == ENTRY_OCCUPIED_BUT_UNSENT
        || table.entries[idx].occupied == ENTRY_OCCUPIED);
  }
}
END_TEST

START_TEST(test_flows_enforce_timestamp_bounds) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 1);

  entry.ip_source = 10;
  time_t later_timestamp = kMySec + FLOW_TABLE_MAX_UPDATE_OFFSET + 1;
  fail_unless(flow_table_process_flow(&table, &entry, later_timestamp)
      == FLOW_ID_ERROR);

  time_t earlier_timestamp = kMySec + FLOW_TABLE_MIN_UPDATE_OFFSET - 1;
  fail_unless(flow_table_process_flow(&table, &entry, earlier_timestamp)
      == FLOW_ID_ERROR);
}
END_TEST

START_TEST(test_flows_can_set_last_update_time) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_unless(flow_table_process_flow(&table, &entry, kMySec)
      == FLOW_ID_FIRST_UNRESERVED);
  fail_unless(table.entries[0].last_update_time_seconds == 0);
  fail_unless(table.num_elements == 1);

  fail_unless(flow_table_process_flow(&table, &entry, kMySec + 60)
      == FLOW_ID_FIRST_UNRESERVED);
  fail_unless(table.entries[0].last_update_time_seconds == 60);
  fail_unless(table.num_elements == 1);

  fail_unless(table.num_expired_flows == 0);
  fail_unless(table.num_dropped_flows == 0);
}
END_TEST

START_TEST(test_flows_can_expire) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 1);

  entry.ip_source = 2;
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 2);

  flows_simulate_update();

  entry.ip_source = 3;
  fail_unless(flow_table_process_flow(
        &table, &entry, kMySec + FLOW_TABLE_EXPIRATION_SECONDS + 1)
      == FLOW_ID_FIRST_UNRESERVED);
  fail_unless(table.num_elements == 1);
  fail_unless(table.entries[0].occupied == ENTRY_OCCUPIED_BUT_UNSENT);
  fail_unless(table.entries[1].occupied == ENTRY_DELETED);
  fail_unless(table.num_expired_flows == 2);
  fail_unless(table.num_dropped_flows == 0);
}
END_TEST

START_TEST(test_flows_can_detect_later_dupes) {
  flow_table_entry_t entry;
  entry.ip_source = 1;
  entry.ip_destination = 2;
  entry.transport_protocol = 3;
  entry.port_source = 4;
  entry.port_destination = 5;
  fail_if(flow_table_process_flow(&table, &entry, kMySec) < 0);
  fail_unless(table.num_elements == 1);

  entry.ip_source = 2;
  fail_if(flow_table_process_flow(&table, &entry, kMySec + 1) < 0);
  fail_unless(table.num_elements == 2);

  flows_simulate_update();

  fail_unless(flow_table_process_flow(
        &table, &entry, kMySec + FLOW_TABLE_EXPIRATION_SECONDS + 1)
      == FLOW_ID_FIRST_UNRESERVED + 1);
  fail_unless(table.num_elements == 1);
  fail_unless(table.entries[0].occupied == ENTRY_DELETED);
  fail_unless(table.entries[1].occupied == ENTRY_OCCUPIED);
  fail_unless(table.num_expired_flows == 1);
}
END_TEST

/********************************************************
 * DNS table tests
 ********************************************************/
static dns_table_t dns_table;

void dns_setup() {
  dns_table_init(&dns_table, NULL);
}

START_TEST(test_dns_adds_a_entries) {
  dns_a_entry_t a_entry;
  a_entry.packet_id = 2;
  a_entry.mac_id = 1;
  a_entry.domain_name = "foo.com";
  a_entry.ip_address = 1234;
  a_entry.ttl = 12345;
  fail_if(dns_table_add_a(&dns_table, &a_entry));
  a_entry.packet_id = 4;
  a_entry.mac_id = 2;
  a_entry.domain_name = "bar.com";
  a_entry.ip_address = 4321;
  a_entry.ttl = 54321;
  fail_if(dns_table_add_a(&dns_table, &a_entry));

  fail_unless(dns_table.a_entries[0].packet_id == 2);
  fail_unless(dns_table.a_entries[0].mac_id == 1);
  fail_if(strcmp(dns_table.a_entries[0].domain_name, "foo.com"));
  fail_unless(dns_table.a_entries[0].ip_address == 1234);
  fail_unless(dns_table.a_entries[0].ttl == 12345);
  fail_unless(dns_table.a_entries[1].packet_id == 4);
  fail_unless(dns_table.a_entries[1].mac_id == 2);
  fail_if(strcmp(dns_table.a_entries[1].domain_name, "bar.com"));
  fail_unless(dns_table.a_entries[1].ip_address == 4321);
  fail_unless(dns_table.a_entries[1].ttl == 54321);
}
END_TEST

START_TEST(test_dns_adds_cname_entries) {
  dns_cname_entry_t cname_entry;
  cname_entry.packet_id = 8;
  cname_entry.mac_id = 1;
  cname_entry.domain_name = "foo.com";
  cname_entry.cname = "gorp.org";
  cname_entry.ttl = 123;
  fail_if(dns_table_add_cname(&dns_table, &cname_entry));
  cname_entry.packet_id = 10;
  cname_entry.mac_id = 2;
  cname_entry.domain_name = "bar.com";
  cname_entry.cname = "baz.net";
  cname_entry.ttl = 321;
  fail_if(dns_table_add_cname(&dns_table, &cname_entry));

  fail_unless(dns_table.cname_entries[0].packet_id == 8);
  fail_unless(dns_table.cname_entries[0].mac_id == 1);
  fail_if(strcmp(
        dns_table.cname_entries[0].domain_name, "foo.com"));
  fail_if(strcmp(
        dns_table.cname_entries[0].cname, "gorp.org"));
  fail_unless(dns_table.cname_entries[0].ttl == 123);
  fail_unless(dns_table.cname_entries[1].packet_id == 10);
  fail_unless(dns_table.cname_entries[1].mac_id == 2);
  fail_if(strcmp(dns_table.cname_entries[1].domain_name, "bar.com"));
  fail_if(strcmp(dns_table.cname_entries[1].cname, "baz.net"));
  fail_unless(dns_table.cname_entries[1].ttl == 321);
}
END_TEST

START_TEST(test_dns_enforces_size) {
  dns_a_entry_t a_entry;
  int a_idx;
  for (a_idx = 0; a_idx < DNS_TABLE_A_ENTRIES; ++a_idx) {
    fail_if(dns_table_add_a(&dns_table, &a_entry));
  }
  fail_unless(dns_table_add_a(&dns_table, &a_entry));

  dns_cname_entry_t cname_entry;
  int cname_idx;
  for (cname_idx = 0; cname_idx < DNS_TABLE_CNAME_ENTRIES; ++cname_idx) {
    fail_if(dns_table_add_cname(&dns_table, &cname_entry));
  }
  fail_unless(dns_table_add_cname(&dns_table, &cname_entry));
}
END_TEST

/********************************************************
 * MAC table tests
 ********************************************************/
static address_table_t address_table;

void mac_setup() {
  address_table_init(&address_table);
}

START_TEST(test_address_can_add_to_table) {
  unsigned char first_mac[6] = "abcdef";
  unsigned char second_mac[6] = "123456";
  uint32_t first_ip = 0x0a000001;
  uint32_t second_ip = 0x0a654321;
  int first_mac_id
    = address_table_lookup(&address_table, first_ip, first_mac);
  fail_unless(first_mac_id >= 0);
  int second_mac_id
    = address_table_lookup(&address_table, second_ip, second_mac);
  fail_unless(second_mac_id >= 0);
  fail_unless(address_table_lookup(
        &address_table, first_ip, first_mac) == first_mac_id);
  fail_unless(address_table_lookup(
        &address_table, second_ip, second_mac) == second_mac_id);
  fail_if(address_table_lookup(
        &address_table, second_ip, first_mac) == first_mac_id);
  fail_if(address_table_lookup(
        &address_table, second_ip, first_mac) == second_mac_id);
  fail_if(address_table_lookup(
        &address_table, first_ip, second_mac) == first_mac_id);
  fail_if(address_table_lookup(
        &address_table, first_ip, second_mac) == second_mac_id);
  fail_unless(address_table_lookup(
        &address_table, first_ip, first_mac) == first_mac_id);
  fail_unless(address_table_lookup(
        &address_table, second_ip, second_mac) == second_mac_id);
}
END_TEST

START_TEST(test_address_can_discard_old_entries) {
  uint8_t mac[ETH_ALEN] = { 1, 2, 3, 4, 5 };
  uint32_t ip = 0x0a123456;
  int first_id = address_table_lookup(&address_table, ip, mac);
  fail_unless(address_table_lookup(&address_table, ip, mac) == first_id);
  int idx;
  for (idx = 1; idx < MAC_TABLE_ENTRIES; ++idx) {
    ++ip;
    address_table_lookup(&address_table, ip, mac);
  }
  ++ip;
  fail_unless(address_table_lookup(&address_table, ip, mac) == first_id);
  ip = 12345;
  fail_unless(address_table_lookup(&address_table, ip, mac) != first_id);
}
END_TEST

#ifdef DISABLE_ANONYMIZATION
START_TEST(test_address_write_update) {
  uint8_t first_mac[ETH_ALEN] = { 1, 2, 3, 4, 5, 6 };
  uint32_t first_ip = 0x0a123456;
  address_table_lookup(&address_table, first_ip, first_mac);
  uint8_t second_mac[ETH_ALEN] = { 6, 5, 4, 3, 2, 1 };
  uint32_t second_ip = 0x0a654321;
  address_table_lookup(&address_table, second_ip, second_mac);

  gzFile handle = open_tempfile();
  address_table_write_update(&address_table, handle);

  int len;
  char* contents = read_tempfile(handle, &len);
  const char* expected_contents = \
      "0 256\n"
      "010203040506 a123456\n"
      "060504030201 a654321\n"
      "\n";
  fail_if(memcmp(expected_contents, contents, len));
  free(contents);
}
END_TEST
#endif

/********************************************************
 * DNS parser tests
 ********************************************************/
int read_trace(const char* filename, uint8_t** contents, int* len) {
  FILE* handle = fopen(filename, "rb");
  if (!handle) {
    perror("Error opening trace file");
    return -1;
  }

  fseek(handle, 0, SEEK_END);
  *len = ftell(handle);
  rewind(handle);

  *contents = (uint8_t *)malloc(sizeof(uint8_t) * (*len));
  if(!*contents) {
    perror("Error allocating buffer for trace");
    return -1;
  }

  fread(*contents, sizeof(uint8_t), *len, handle);
  fclose(handle);
  return 0;
}

START_TEST(test_dns_parser_can_parse_valid_responses) {
  static char* traces[] = { "test_traces/gatech.edu.success" };

  int idx;
  for (idx = 0; idx < sizeof(traces) / sizeof(traces[0]); ++idx) {
    uint8_t *contents;
    int len;
    fail_if(read_trace(traces[idx], &contents, &len));
    fail_if(process_dns_packet(contents, len, &dns_table, 0, 1));
    free(contents);
  }
}
END_TEST

START_TEST(test_dns_parser_fails_on_invalid_responses) {
  static char* traces[] = {
    "test_traces/gatech.edu.missing_body",
    "test_traces/gatech.edu.missing_answer",
    "test_traces/gatech.edu.missing_additional",
    "test_traces/gatech.edu.missing_additional_record",
    "test_traces/gatech.edu.missing_answer_address",
    "test_traces/gatech.edu.missing_partial_rr_header",
    "test_traces/gatech.edu.malformed_size"
  };

  int idx;
  for (idx = 0; idx < sizeof(traces) / sizeof(traces[0]); ++idx) {
    uint8_t *contents;
    int len;
    fail_if(read_trace(traces[idx], &contents, &len));
    fail_unless(process_dns_packet(contents, len, &dns_table, 0, 1));
    free(contents);
  }
}
END_TEST

/********************************************************
 * Utility functions
 ********************************************************/
START_TEST(test_util_is_ip_private) {
  char *private_addresses[] = {
    "192.168.0.1",
    "10.0.0.1",
    "192.168.254.254",
    "10.254.254.254",
    "192.168.142.182",
    "172.16.0.1",
    "172.31.254.254",
    "172.16.17.21"
  };

  char *public_addresses[] = {
    "143.215.129.51",
    "8.8.8.8",
    "1.2.3.4",
    "11.0.0.1",
    "9.254.254.254",
    "192.167.254.254",
    "192.169.0.1"
  };

  int idx;
  for (idx = 0;
       idx < sizeof(private_addresses)/sizeof(private_addresses[0]);
       ++idx) {
    unsigned char buf[sizeof(struct in_addr)];
    fail_unless(inet_pton(AF_INET, private_addresses[idx], buf) > 0);
    fail_unless(is_address_private(ntohl(*(uint32_t *)buf)));
  }

  for (idx = 0;
       idx < sizeof(public_addresses)/sizeof(public_addresses[0]);
       ++idx) {
    unsigned char buf[sizeof(struct in_addr)];
    fail_unless(inet_pton(AF_INET, public_addresses[idx], buf) > 0);
    fail_if(is_address_private(ntohl(*(uint32_t *)buf)));
  }
}
END_TEST

/********************************************************
 * Whitelist tests
 ********************************************************/
START_TEST(test_whitelist_can_lookup) {
  const char* contents =
    "foo.com\n"
    "bar.org\n"
    "gorp.edu";

  domain_whitelist_t whitelist;
  domain_whitelist_load(&whitelist, contents);

  fail_if(domain_whitelist_lookup(&whitelist, "foo.com"));
  fail_if(domain_whitelist_lookup(&whitelist, "bar.org"));
  fail_if(domain_whitelist_lookup(&whitelist, "gorp.edu"));

  fail_if(domain_whitelist_lookup(&whitelist, "www.foo.com"));
  fail_if(domain_whitelist_lookup(&whitelist, "mail.cs.gorp.edu"));

  fail_unless(domain_whitelist_lookup(&whitelist, ""));
  fail_unless(domain_whitelist_lookup(&whitelist, "foobar.org"));
  fail_unless(domain_whitelist_lookup(&whitelist, "www.foobar.org"));
  fail_unless(domain_whitelist_lookup(&whitelist, "org"));
  fail_unless(domain_whitelist_lookup(&whitelist, ".org"));
  fail_unless(domain_whitelist_lookup(&whitelist, "ar.org"));

  domain_whitelist_destroy(&whitelist);

#ifdef _BLOOM_WHITELIST_H
  /* test Bloom Whitelist */
  bloom_whitelist_t bloom_whitelist;
  bloom_whitelist_init(&bloom_whitelist);

  fail_if(bloom_whitelist_lookup(&bloom_whitelist, "saudieng.net"));
  fail_if(bloom_whitelist_lookup(&bloom_whitelist, "thick-click.com"));
  fail_if(bloom_whitelist_lookup(&bloom_whitelist, "webfo.biz"));

  fail_unless(bloom_whitelist_lookup(&bloom_whitelist, ""));
  fail_unless(bloom_whitelist_lookup(&bloom_whitelist, "google.com"));
#endif

}
END_TEST



/********************************************************
 * Device throughput table
 ********************************************************/

device_throughput_table_t device_throughput_table;

START_TEST(test_device_throughput_table) {
  device_throughput_table_init(&device_throughput_table);

}
END_TEST

/********************************************************
 * Test setup
 ********************************************************/
Suite* build_suite() {
  Suite *s = suite_create("Bismark passive");

  TCase *tc_series = tcase_create("Packet series");
  tcase_add_checked_fixture(tc_series, series_setup, NULL);
  tcase_add_test(tc_series, test_series_add);
  tcase_add_test(tc_series, test_series_overflow);
  tcase_add_test(tc_series, test_series_write_update);
  suite_add_tcase(s, tc_series);

  TCase *tc_flows = tcase_create("Flow table");
  tcase_add_checked_fixture(tc_flows, flows_setup, NULL);
  tcase_add_test(tc_flows, test_flows_detect_dupes);
  tcase_add_test(tc_flows, test_flows_can_probe);
  tcase_add_test(tc_flows, test_flows_can_set_base_timestamp);
  tcase_add_test(tc_flows, test_flows_can_advance_base_timestamp);
  tcase_add_test(tc_flows, test_flows_enforce_timestamp_bounds);
  tcase_add_test(tc_flows, test_flows_can_set_last_update_time);
  tcase_add_test(tc_flows, test_flows_can_expire);
  tcase_add_test(tc_flows, test_flows_can_detect_later_dupes);
  suite_add_tcase(s, tc_flows);

  TCase *tc_dns = tcase_create("DNS table");
  tcase_add_checked_fixture(tc_dns, dns_setup, NULL);
  tcase_add_test(tc_dns, test_dns_adds_a_entries);
  tcase_add_test(tc_dns, test_dns_adds_cname_entries);
  tcase_add_test(tc_dns, test_dns_enforces_size);
  suite_add_tcase(s, tc_dns);

  TCase *tc_address = tcase_create("MAC table");
  tcase_add_checked_fixture(tc_address, mac_setup, NULL);
  tcase_add_test(tc_address, test_address_can_add_to_table);
  tcase_add_test(tc_address, test_address_can_discard_old_entries);
#ifdef DISABLE_ANONYMIZATION
  tcase_add_test(tc_address, test_address_write_update);
#endif
  suite_add_tcase(s, tc_address);

  TCase *tc_dns_parser = tcase_create("DNS parser");
  tcase_add_checked_fixture(tc_dns_parser, dns_setup, NULL);
  tcase_add_test(tc_dns_parser, test_dns_parser_can_parse_valid_responses);
  tcase_add_test(tc_dns_parser, test_dns_parser_fails_on_invalid_responses);
  suite_add_tcase(s, tc_dns_parser);

  TCase *tc_util = tcase_create("Utilities");
  tcase_add_test(tc_util, test_util_is_ip_private);
  suite_add_tcase(s, tc_util);

  TCase *tc_whitelist = tcase_create("Whitelist");
  tcase_add_test(tc_whitelist, test_whitelist_can_lookup);
  suite_add_tcase(s, tc_whitelist);

  return s;
}

int main(int argc, char* argv[]) {
  int number_failed;
  Suite *s = build_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
