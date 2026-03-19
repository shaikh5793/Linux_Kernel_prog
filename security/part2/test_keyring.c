/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * test_keyring.c - Keyring Functionality Test Suite
 *
 * Automated test program to verify correctness of keyring operations.
 * Tests basic operations and edge cases to ensure the keyring subsystem
 * works as expected.
 *
 * TEST CATEGORIES:
 * 1. Key Creation - Verify keys can be created successfully
 * 2. Key Reading - Verify payload can be retrieved correctly
 * 3. Key Searching - Verify keys can be found by description
 * 4. Key Update - Verify existing keys can be updated
 * 5. Key Permissions - Verify permission checks work
 * 6. Key Linking - Verify keys can be linked between keyrings
 * 7. Key Revocation - Verify revoked keys become inaccessible
 *
 * TEST METHODOLOGY:
 * - Each test is independent
 * - Tests clean up after themselves (unlink keys)
 * - Failures include errno information for debugging
 * - Summary printed at end showing pass/fail count
 *
 * RUNNING TESTS:
 *   gcc -o test_keyring test_keyring.c -lkeyutils
 *   ./test_keyring
 *
 * Expected output: All tests should PASS
 * If tests FAIL, check:
 * - Keyring subsystem enabled in kernel (CONFIG_KEYS=y)
 * - libkeyutils installed
 * - Sufficient permissions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <keyutils.h>
#include <errno.h>

#define TEST_PASS printf("  [PASS]\n")
#define TEST_FAIL printf("  [FAIL]\n")

static int tests_passed = 0;
static int tests_failed = 0;

/*
 * test_create_key - Test basic key creation
 *
 * Verifies that:
 * - add_key() succeeds and returns a valid key ID (> 0)
 * - Key can be unlinked (cleanup)
 *
 * This is the most fundamental operation - if this fails,
 * something is seriously wrong with the keyring subsystem.
 */
static void test_create_key(void)
{
	key_serial_t key;

	printf("Test: Create user key... ");

	/*
	 * Create a simple user-type key
	 * Should succeed for any user (no special permissions needed)
	 */
	key = add_key("user", "test_key", "test_data", 9,
	              KEY_SPEC_SESSION_KEYRING);

	if (key > 0) {
		TEST_PASS;
		tests_passed++;
		/* Clean up: remove key from keyring */
		keyctl_unlink(key, KEY_SPEC_SESSION_KEYRING);
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Error: %s\n", strerror(errno));
	}
}

/*
 * test_read_key - Test key payload reading
 *
 * Verifies that:
 * - Key payload can be read back correctly
 * - Data matches what was stored
 * - Buffer handling works properly
 *
 * Tests the round-trip: create key → read key → verify data
 */
static void test_read_key(void)
{
	key_serial_t key;
	char buffer[256];
	const char *data = "read_test_data";
	long ret;

	printf("Test: Read key data... ");

	/* Create test key */
	key = add_key("user", "read_test", data, strlen(data),
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}

	/*
	 * Read back the payload
	 * Should return exact same data we put in
	 */
	ret = keyctl_read(key, buffer, sizeof(buffer));
	if (ret > 0) {
		buffer[ret] = '\0';
		/* Verify data integrity */
		if (strcmp(buffer, data) == 0) {
			TEST_PASS;
			tests_passed++;
		} else {
			TEST_FAIL;
			tests_failed++;
			printf("    Data mismatch: expected '%s', got '%s'\n",
			       data, buffer);
		}
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Error: %s\n", strerror(errno));
	}

	/* Cleanup */
	keyctl_unlink(key, KEY_SPEC_SESSION_KEYRING);
}

/*
 * test_search_key - Test key search
 */
static void test_search_key(void)
{
	key_serial_t key, found;
	const char *desc = "search_test";
	
	printf("Test: Search for key... ");
	
	key = add_key("user", desc, "data", 4, KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	found = keyctl_search(KEY_SPEC_SESSION_KEYRING, "user", desc, 0);
	if (found == key) {
		TEST_PASS;
		tests_passed++;
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Found wrong key: expected %d, got %d\n", key, found);
	}
	
	keyctl_unlink(key, KEY_SPEC_SESSION_KEYRING);
}

/*
 * test_key_permissions - Test permission modifications
 */
static void test_key_permissions(void)
{
	key_serial_t key;
	key_perm_t perm = KEY_POS_ALL;
	
	printf("Test: Set key permissions... ");
	
	key = add_key("user", "perm_test", "data", 4,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	if (keyctl_setperm(key, perm) == 0) {
		TEST_PASS;
		tests_passed++;
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Error: %s\n", strerror(errno));
	}
	
	keyctl_unlink(key, KEY_SPEC_SESSION_KEYRING);
}

/*
 * test_key_timeout - Test key expiration
 */
static void test_key_timeout(void)
{
	key_serial_t key;
	char buffer[256];
	
	printf("Test: Set key timeout... ");
	
	key = add_key("user", "timeout_test", "data", 4,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	if (keyctl_set_timeout(key, 2) == 0) {
		TEST_PASS;
		tests_passed++;
		
		printf("  Waiting for key to expire (2 seconds)... ");
		sleep(3);
		
		/* Try to read expired key */
		if (keyctl_read(key, buffer, sizeof(buffer)) < 0) {
			printf("expired as expected\n");
		} else {
			printf("key still readable (unexpected)\n");
		}
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Error: %s\n", strerror(errno));
		keyctl_unlink(key, KEY_SPEC_SESSION_KEYRING);
	}
}

/*
 * test_key_revoke - Test key revocation
 */
static void test_key_revoke(void)
{
	key_serial_t key;
	char buffer[256];
	
	printf("Test: Revoke key... ");
	
	key = add_key("user", "revoke_test", "data", 4,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	if (keyctl_revoke(key) == 0) {
		/* Try to read revoked key (should fail) */
		if (keyctl_read(key, buffer, sizeof(buffer)) < 0) {
			TEST_PASS;
			tests_passed++;
		} else {
			TEST_FAIL;
			tests_failed++;
			printf("    Revoked key is still readable\n");
		}
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Error: %s\n", strerror(errno));
	}
	
	keyctl_unlink(key, KEY_SPEC_SESSION_KEYRING);
}

/*
 * test_key_linking - Test key linking between keyrings
 */
static void test_key_linking(void)
{
	key_serial_t session, process, key;
	
	printf("Test: Link key between keyrings... ");
	
	session = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);
	process = keyctl_get_keyring_ID(KEY_SPEC_PROCESS_KEYRING, 1);
	
	if (session < 0 || process < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	key = add_key("user", "link_test", "data", 4, session);
	if (key < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	if (keyctl_link(key, process) == 0) {
		if (keyctl_unlink(key, process) == 0) {
			TEST_PASS;
			tests_passed++;
		} else {
			TEST_FAIL;
			tests_failed++;
			printf("    Failed to unlink\n");
		}
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Failed to link\n");
	}
	
	keyctl_unlink(key, session);
}

/*
 * test_keyring_clear - Test clearing keyring
 */
static void test_keyring_clear(void)
{
	key_serial_t process, key1, key2;
	char buffer[16384];
	long ret;
	
	printf("Test: Clear keyring... ");
	
	process = keyctl_get_keyring_ID(KEY_SPEC_PROCESS_KEYRING, 1);
	if (process < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	/* Add some keys */
	key1 = add_key("user", "clear_test1", "data", 4, process);
	key2 = add_key("user", "clear_test2", "data", 4, process);
	
	if (key1 < 0 || key2 < 0) {
		TEST_FAIL;
		tests_failed++;
		return;
	}
	
	/* Clear the keyring */
	if (keyctl_clear(process) == 0) {
		/* Check if keyring is empty */
		ret = keyctl_read(process, buffer, sizeof(buffer));
		if (ret == 0) {
			TEST_PASS;
			tests_passed++;
		} else {
			TEST_FAIL;
			tests_failed++;
			printf("    Keyring not empty after clear\n");
		}
	} else {
		TEST_FAIL;
		tests_failed++;
		printf("    Error: %s\n", strerror(errno));
	}
}

int main(int argc, char *argv[])
{
	printf("=========================================\n");
	printf("   Keyring Functionality Test Suite\n");
	printf("=========================================\n\n");
	
	printf("Running tests...\n\n");
	
	test_create_key();
	test_read_key();
	test_search_key();
	test_key_permissions();
	test_key_timeout();
	test_key_revoke();
	test_key_linking();
	test_keyring_clear();
	
	printf("\n=========================================\n");
	printf("Test Results:\n");
	printf("  Passed: %d\n", tests_passed);
	printf("  Failed: %d\n", tests_failed);
	printf("  Total:  %d\n", tests_passed + tests_failed);
	printf("=========================================\n\n");
	
	if (tests_failed == 0) {
		printf("All tests passed!\n\n");
		return 0;
	} else {
		printf("Some tests failed.\n\n");
		return 1;
	}
}

