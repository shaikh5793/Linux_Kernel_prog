/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * keyctl.c - Advanced Keyctl Operations Demonstration
 *
 * Demonstrates advanced keyring management features beyond basic
 * create/read/search operations.
 *
 * ADVANCED OPERATIONS DEMONSTRATED:
 * 1. Key Timeouts - Automatic expiration after specified duration
 * 2. Permissions - Fine-grained access control (like file permissions)
 * 3. Ownership - Changing key UID/GID
 * 4. Linking - Adding keys to multiple keyrings
 * 5. Unlinking - Removing keys from keyrings
 * 6. Revocation - Permanently invalidating keys
 * 7. Invalidation - More thorough revocation
 *
 * KEY LIFECYCLE:
 * Create → Use → (Optional: Link/Timeout/Chown) → Revoke/Expire → Garbage Collect
 *
 * PERMISSION MODEL:
 * Keys use 4-level permissions similar to files:
 * - Possessor: Process that currently possesses the key
 * - User: Owner (UID) of the key
 * - Group: Group (GID) of the key
 * - Other: Everyone else
 *
 * Each level can have these permissions (bitwise OR):
 * - VIEW (0x01): See that key exists
 * - READ (0x02): Read key payload
 * - WRITE (0x04): Update key payload
 * - SEARCH (0x08): Find key in searches
 * - LINK (0x10): Link key into keyrings
 * - SETATTR (0x20): Change key attributes
 *
 * REVOCATION vs INVALIDATION:
 * - Revoke: Mark key as revoked, prevent further access
 * - Invalidate: Like revoke but also removes from keyrings
 * - Both are irreversible!
 *
 * PRACTICAL USE CASES:
 * - Timeout: Session tokens that expire automatically
 * - Permissions: Restrict key access to specific users/groups
 * - Linking: Share keys between process and session keyrings
 * - Revocation: Invalidate compromised credentials
 *
 * USAGE:
 *   gcc -o keyctl keyctl.c -lkeyutils
 *   ./keyctl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <keyutils.h>  /* libkeyutils API */
#include <time.h>       /* For timeout demonstration */

/*
 * show_key_timeout - Demonstrate automatic key expiration
 *
 * Shows how to set a timeout on a key so it automatically expires
 * after a specified duration.
 *
 * TIMEOUT BEHAVIOR:
 * - Timeout is in seconds from when set (not from creation)
 * - After timeout expires, key is automatically revoked
 * - Expired keys are garbage collected by kernel
 * - Timeout can be updated before it expires
 * - Setting timeout to 0 clears any existing timeout
 *
 * USE CASES:
 * - Session tokens that should expire after inactivity
 * - Temporary credentials for short-lived operations
 * - Cache entries with TTL (time-to-live)
 * - OAuth access tokens with expiration
 *
 * IMPLEMENTATION:
 * The kernel maintains a timer for each key with a timeout.
 * When timer fires, kernel automatically calls revoke on the key.
 * This happens in kernel context - no userspace involvement needed.
 */
static void show_key_timeout(void)
{
	key_serial_t key;
	time_t timeout = 10; /* 10 seconds */

	printf("=== Key Timeout Demo ===\n\n");

	/* Create a temporary key */
	key = add_key("user", "temp_key", "temporary_data", 14,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		perror("add_key");
		return;
	}

	printf("Created temporary key ID: %d\n", key);

	/*
	 * Set timeout for automatic expiration
	 * - timeout: Seconds from now until key expires
	 * - After expiration, key is revoked automatically
	 * - Key becomes inaccessible and is garbage collected
	 *
	 * Requires SETATTR permission on the key
	 */
	if (keyctl_set_timeout(key, timeout) < 0) {
		perror("keyctl_set_timeout");
		return;
	}

	printf("Set timeout: %ld seconds\n", timeout);
	printf("Key will expire automatically\n");
	printf("After expiration, key will be revoked\n\n");
}

/*
 * show_key_permissions - Demonstrate permission management
 */
static void show_key_permissions(void)
{
	key_serial_t key;
	key_perm_t perm;
	
	printf("=== Key Permissions Demo ===\n\n");
	
	/* Create a key */
	key = add_key("user", "perm_key", "permission_test", 15,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		perror("add_key");
		return;
	}
	
	printf("Created key ID: %d\n", key);
	
	/* Set restrictive permissions - possessor only */
	perm = KEY_POS_VIEW | KEY_POS_READ | KEY_POS_WRITE | 
	       KEY_POS_SEARCH | KEY_POS_LINK | KEY_POS_SETATTR;
	
	if (keyctl_setperm(key, perm) < 0) {
		perror("keyctl_setperm");
		return;
	}
	
	printf("Set permissions: 0x%08x (possessor only)\n", perm);
	printf("  Possessor: VIEW READ WRITE SEARCH LINK SETATTR\n");
	printf("  User: none\n");
	printf("  Group: none\n");
	printf("  Other: none\n\n");
	
	/* Now set more open permissions */
	perm = KEY_POS_ALL | KEY_USR_VIEW | KEY_USR_READ;
	
	if (keyctl_setperm(key, perm) < 0) {
		perror("keyctl_setperm");
		return;
	}
	
	printf("Updated permissions: 0x%08x\n", perm);
	printf("  Possessor: all permissions\n");
	printf("  User: VIEW READ\n\n");
}

/*
 * show_key_ownership - Demonstrate ownership changes
 */
static void show_key_ownership(void)
{
	key_serial_t key;
	uid_t uid = getuid();
	gid_t gid = getgid();
	
	printf("=== Key Ownership Demo ===\n\n");
	
	key = add_key("user", "owner_key", "ownership_test", 14,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		perror("add_key");
		return;
	}
	
	printf("Created key ID: %d\n", key);
	printf("Current UID: %d, GID: %d\n", uid, gid);
	
	/* Try to change ownership (requires appropriate privileges) */
	if (keyctl_chown(key, uid, gid) < 0) {
		/* May fail if not root or not owner */
		printf("Cannot change ownership (not root)\n");
	} else {
		printf("Successfully set ownership to UID:%d, GID:%d\n", uid, gid);
	}
	printf("\n");
}

/*
 * show_key_linking - Demonstrate key linking between keyrings
 *
 * Shows how a single key can be linked into multiple keyrings,
 * similar to hard links in filesystems.
 *
 * KEY LINKING CONCEPTS:
 * - A key can exist in multiple keyrings simultaneously
 * - Each keyring just holds a reference to the key
 * - The key itself exists only once in kernel memory
 * - Like hard links: same content, multiple references
 *
 * REFERENCE COUNTING:
 * - Each link increments key's reference count
 * - Unlinking decrements reference count
 * - Key is freed only when:
 *   1. Reference count reaches zero AND
 *   2. No processes hold possession AND
 *   3. Key is expired/revoked
 *
 * WHY LINK KEYS:
 * - Share secrets between process and session
 * - Make key available in multiple search paths
 * - Control key lifetime (unlink to remove from keyring)
 * - Organize keys logically in different keyrings
 *
 * LINK vs ADD:
 * - add_key(): Creates new key OR updates existing
 * - keyctl_link(): Adds existing key to another keyring
 * - Link doesn't create a copy - same key, multiple locations
 *
 * PERMISSIONS:
 * - Requires LINK permission on the key
 * - Requires WRITE permission on destination keyring
 */
static void show_key_linking(void)
{
	key_serial_t session, process, key;

	printf("=== Key Linking Demo ===\n\n");

	/*
	 * Get or create keyring IDs
	 * Second parameter (1) means create if doesn't exist
	 */
	session = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);
	process = keyctl_get_keyring_ID(KEY_SPEC_PROCESS_KEYRING, 1);

	if (session < 0 || process < 0) {
		perror("keyctl_get_keyring_ID");
		return;
	}

	printf("Session keyring: %d\n", session);
	printf("Process keyring: %d\n\n", process);

	/* Create key in session keyring */
	key = add_key("user", "linked_key", "link_test", 9, session);
	if (key < 0) {
		perror("add_key");
		return;
	}

	printf("Created key %d in session keyring\n", key);
	printf("Reference count: 1 (session keyring)\n\n");

	/*
	 * Link existing key into process keyring
	 * This doesn't copy the key - creates another reference
	 * Now the same key is accessible from both keyrings
	 */
	if (keyctl_link(key, process) < 0) {
		perror("keyctl_link");
		return;
	}

	printf("Linked key %d to process keyring\n", key);
	printf("Reference count: 2 (session + process keyrings)\n");
	printf("Key is now accessible from both keyrings\n\n");

	/*
	 * Unlink from process keyring
	 * Removes the reference but key still exists in session keyring
	 * Like removing a hard link - other links remain valid
	 */
	if (keyctl_unlink(key, process) < 0) {
		perror("keyctl_unlink");
		return;
	}

	printf("Unlinked key %d from process keyring\n", key);
	printf("Reference count: 1 (session keyring only)\n");
	printf("Key remains in session keyring\n\n");
}

/*
 * show_key_revocation - Demonstrate key revocation
 */
static void show_key_revocation(void)
{
	key_serial_t key;
	char buffer[256];
	
	printf("=== Key Revocation Demo ===\n\n");
	
	/* Create a key */
	key = add_key("user", "revoke_key", "will_be_revoked", 15,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		perror("add_key");
		return;
	}
	
	printf("Created key ID: %d\n", key);
	
	/* Read key (should work) */
	if (keyctl_read(key, buffer, sizeof(buffer)) > 0) {
		printf("Key data before revocation: %s\n", buffer);
	}
	
	/* Revoke the key */
	if (keyctl_revoke(key) < 0) {
		perror("keyctl_revoke");
		return;
	}
	
	printf("Revoked key %d\n", key);
	
	/* Try to read again (should fail) */
	if (keyctl_read(key, buffer, sizeof(buffer)) < 0) {
		printf("Cannot read revoked key (expected)\n");
	}
	
	printf("Revoked keys cannot be accessed\n\n");
}

/*
 * show_key_invalidate - Demonstrate key invalidation
 */
static void show_key_invalidate(void)
{
	key_serial_t key;
	
	printf("=== Key Invalidation Demo ===\n\n");
	
	key = add_key("user", "invalid_key", "will_be_invalid", 15,
	              KEY_SPEC_SESSION_KEYRING);
	if (key < 0) {
		perror("add_key");
		return;
	}
	
	printf("Created key ID: %d\n", key);
	
	/* Invalidate the key */
	if (keyctl_invalidate(key) < 0) {
		perror("keyctl_invalidate");
		printf("Note: invalidate may not be available on all systems\n");
	} else {
		printf("Invalidated key %d\n", key);
		printf("Invalidation is similar to revocation but more thorough\n");
	}
	printf("\n");
}

/*
 * show_keyring_contents - Display all keys in a keyring
 */
static void show_keyring_contents(key_serial_t keyring, const char *name)
{
	char buffer[16384];
	key_serial_t *keys;
	long ret, i, count;
	char desc[256];
	
	printf("Contents of %s keyring (ID: %d):\n", name, keyring);
	
	ret = keyctl_read(keyring, buffer, sizeof(buffer));
	if (ret < 0) {
		perror("keyctl_read");
		return;
	}
	
	keys = (key_serial_t *)buffer;
	count = ret / sizeof(key_serial_t);
	
	printf("  Total keys: %ld\n", count);
	
	for (i = 0; i < count; i++) {
		ret = keyctl_describe(keys[i], desc, sizeof(desc));
		if (ret > 0) {
			desc[ret] = '\0';
			printf("    [%ld] ID:%d - %s\n", i + 1, keys[i], desc);
		}
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	key_serial_t session;
	
	printf("========================================\n");
	printf("  Advanced Keyctl Operations Demo\n");
	printf("========================================\n\n");
	
	printf("Process: %s (PID: %d)\n", argv[0], getpid());
	printf("UID: %d, GID: %d\n\n", getuid(), getgid());
	
	/* Get session keyring */
	session = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);
	if (session < 0) {
		perror("keyctl_get_keyring_ID");
		return 1;
	}
	
	/* Run demonstrations */
	show_key_timeout();
	show_key_permissions();
	show_key_ownership();
	show_key_linking();
	show_key_revocation();
	show_key_invalidate();
	
	/* Show final state */
	printf("=== Final Keyring State ===\n\n");
	show_keyring_contents(session, "session");
	
	printf("=== Cleanup Commands ===\n\n");
	printf("View all keys:\n");
	printf("  keyctl show\n\n");
	
	printf("Clear session keyring:\n");
	printf("  keyctl clear @s\n\n");
	
	printf("Remove specific key:\n");
	printf("  keyctl unlink <key_id> @s\n\n");
	
	return 0;
}

