/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * key_ops.c - Userspace Key Operations Demonstration
 *
 * Demonstrates fundamental keyring operations from userspace using
 * the keyctl system call interface and libkeyutils library.
 *
 * SYSTEM CALLS USED:
 * - add_key():     Create/update a key and add to keyring
 * - keyctl():      Multiplex operation for key management
 * - request_key(): Search for and retrieve a key
 *
 * KEY OPERATIONS DEMONSTRATED:
 * 1. Creating keys in different keyrings (session, process, user)
 * 2. Reading key payload data
 * 3. Searching for keys by description
 * 4. Listing keyring contents
 * 5. Inspecting key metadata
 *
 * KEYRING SPECIFIERS (@-notation):
 * - @s  = Session keyring (KEY_SPEC_SESSION_KEYRING)
 * - @p  = Process keyring (KEY_SPEC_PROCESS_KEYRING)
 * - @t  = Thread keyring (KEY_SPEC_THREAD_KEYRING)
 * - @u  = User keyring (KEY_SPEC_USER_KEYRING)
 * - @us = User session keyring (KEY_SPEC_USER_SESSION_KEYRING)
 *
 * PRACTICAL USE CASES:
 * - Store authentication tokens for network services
 * - Cache credentials for filesystem operations
 * - Share secrets between related processes
 * - Temporary storage for encryption keys
 *
 * SECURITY NOTES:
 * - Keys are stored in kernel memory, not swapped to disk
 * - Keys can have timeouts and automatic expiration
 * - Access controlled by permissions (like files)
 * - Keys can be revoked to invalidate all references
 *
 * USAGE:
 *   gcc -o key_ops key_ops.c -lkeyutils
 *   ./key_ops
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <keyutils.h>  /* libkeyutils - high-level keyring API */

/*
 * create_key - Create a key in specified keyring
 * @type: Key type (e.g., "user", "logon", "keyring")
 * @desc: Human-readable description (used for searching)
 * @data: Payload data to store in the key
 * @keyring: Keyring ID to add this key to
 *
 * Creates a new key using the add_key() system call.
 *
 * KEY TYPES:
 * - "user": General-purpose, readable from userspace
 * - "logon": Like user but payload not readable (secure)
 * - "keyring": Container for other keys
 *
 * The add_key() system call:
 * 1. Allocates a new key structure in kernel
 * 2. Copies payload data to kernel memory
 * 3. Links key into specified keyring
 * 4. Returns unique serial number (key ID)
 *
 * If a key with same type+description exists in the keyring,
 * it gets updated instead of creating a duplicate.
 *
 * Return: Key ID on success, -1 on error
 */
static int create_key(const char *type, const char *desc,
                      const char *data, key_serial_t keyring)
{
	key_serial_t key;

	printf("Creating key:\n");
	printf("  Type: %s\n", type);
	printf("  Description: %s\n", desc);
	printf("  Data: %s\n", data);
	printf("  Keyring: %d\n", keyring);

	/*
	 * add_key() system call:
	 * - type: Type of key to create
	 * - desc: Description string (searchable)
	 * - data: Payload bytes
	 * - strlen(data): Payload length
	 * - keyring: Destination keyring (or KEY_SPEC_* constant)
	 *
	 * Returns: Key serial number (positive) or -1 on error
	 */
	key = add_key(type, desc, data, strlen(data), keyring);
	if (key < 0) {
		perror("add_key");
		return -1;
	}

	printf("  Created key ID: %d\n\n", key);
	return key;
}

/*
 * read_key - Read and display key payload data
 * @key: Key ID to read
 *
 * Reads the payload (secret data) from a key using keyctl_read().
 *
 * IMPORTANT:
 * - Only works for "user" type keys
 * - "logon" type keys cannot be read (security feature)
 * - Requires READ permission on the key
 * - Payload is copied from kernel to userspace
 *
 * SECURITY:
 * The kernel keeps key data in non-swappable memory.
 * When read, data is copied to userspace buffer which
 * CAN be swapped. For sensitive data, consider:
 * - Using mlock() on the buffer
 * - Zeroing buffer after use
 * - Using "logon" type instead of "user"
 *
 * Return: 0 on success, -1 on error
 */
static int read_key(key_serial_t key)
{
	char buffer[4096];
	long ret;

	printf("Reading key ID %d:\n", key);

	/*
	 * keyctl_read() retrieves key payload
	 * Returns: Number of bytes read, or -1 on error
	 *
	 * If buffer is NULL, returns required size
	 * Useful for querying payload size before allocation
	 */
	ret = keyctl_read(key, buffer, sizeof(buffer) - 1);
	if (ret < 0) {
		perror("keyctl_read");
		return -1;
	}

	buffer[ret] = '\0';
	printf("  Data: %s\n", buffer);
	printf("  Length: %ld bytes\n\n", ret);

	return 0;
}

/*
 * search_key - Search for a key by type and description
 * @keyring: Keyring to search (or KEY_SPEC_* for hierarchy search)
 * @type: Key type to match
 * @desc: Description string to match
 *
 * Searches for a key matching the given type and description.
 *
 * SEARCH BEHAVIOR:
 * - If keyring is a specific keyring ID: searches only that keyring
 * - If keyring is KEY_SPEC_*: searches the keyring hierarchy
 *   (e.g., KEY_SPEC_PROCESS_KEYRING searches: thread → process → session)
 *
 * MATCHING:
 * - Type must match exactly
 * - Description must match exactly (case-sensitive)
 * - Returns first match found
 *
 * POSSESSION:
 * The last parameter (0) is dest_keyring for request_key() behavior.
 * 0 means don't install found key anywhere specific.
 *
 * Return: Key ID if found, -1 if not found or error
 */
static int search_key(key_serial_t keyring, const char *type,
                      const char *desc)
{
	key_serial_t key;

	printf("Searching for key:\n");
	printf("  Keyring: %d\n", keyring);
	printf("  Type: %s\n", type);
	printf("  Description: %s\n", desc);

	/*
	 * keyctl_search() searches keyring for matching key
	 * - Searches specified keyring and (optionally) children
	 * - Can search keyring hierarchy if given KEY_SPEC_*
	 * - Returns key ID if found, -1 if not found
	 */
	key = keyctl_search(keyring, type, desc, 0);
	if (key < 0) {
		perror("keyctl_search");
		return -1;
	}

	printf("  Found key ID: %d\n\n", key);
	return key;
}

/*
 * show_key_info - Display key metadata and description
 * @key: Key ID to describe
 *
 * Retrieves and displays key metadata using keyctl_describe().
 *
 * DESCRIPTION FORMAT:
 * The kernel returns a semicolon-separated string:
 *   type;uid;gid;perm;description
 *
 * Example: "user;1000;1000;3f010000;mykey"
 *
 * PERMISSIONS:
 * Permission format is 8 hex digits (32 bits):
 * - Bits 0-7:   Possessor permissions
 * - Bits 8-15:  User permissions
 * - Bits 16-23: Group permissions
 * - Bits 24-31: Other permissions
 *
 * Each 8-bit group has flags for:
 * - VIEW (0x01), READ (0x02), WRITE (0x04), SEARCH (0x08),
 * - LINK (0x10), SETATTR (0x20)
 *
 * Common permission: 0x3f010000
 * = Possessor: all (0x3f)
 * = User: view (0x01)
 * = Group: none (0x00)
 * = Other: none (0x00)
 */
static void show_key_info(key_serial_t key)
{
	char buffer[4096];
	long ret;

	printf("Key ID %d information:\n", key);

	/*
	 * keyctl_describe() returns formatted metadata string
	 * Does NOT return the payload - only descriptive info
	 * Requires VIEW permission on the key
	 */
	ret = keyctl_describe(key, buffer, sizeof(buffer) - 1);
	if (ret < 0) {
		perror("keyctl_describe");
		return;
	}

	buffer[ret] = '\0';
	printf("  Description: %s\n", buffer);

	/*
	 * Parse the semicolon-delimited fields
	 * Format: type;uid;gid;perm;description
	 */
	char *type = strtok(buffer, ";");
	char *uid = strtok(NULL, ";");
	char *gid = strtok(NULL, ";");
	char *perm = strtok(NULL, ";");
	char *desc = strtok(NULL, ";");

	if (type && uid && gid && perm && desc) {
		printf("  Type: %s\n", type);
		printf("  Owner UID: %s\n", uid);
		printf("  Owner GID: %s\n", gid);
		printf("  Permissions: %s (hex)\n", perm);
		printf("  Description: %s\n", desc);
	}

	printf("\n");
}

/*
 * list_keyring - List all keys contained in a keyring
 * @keyring: Keyring ID to list
 * @name: Human-readable name for display
 *
 * Lists all keys linked into a keyring using keyctl_read().
 *
 * HOW IT WORKS:
 * A keyring is a special type of key that contains other keys.
 * When you "read" a keyring (not a regular key), you get:
 * - Array of key_serial_t values (4 bytes each)
 * - Each value is the ID of a key linked into this keyring
 *
 * KEYRINGS AS CONTAINERS:
 * Think of a keyring like a directory and keys like files:
 * - A key can be linked into multiple keyrings (like hard links)
 * - Unlinking from last keyring doesn't delete key immediately
 * - Key deleted only when last reference dropped AND timeout expires
 *
 * READING KEYRINGS:
 * - Requires READ permission on the keyring
 * - Returns array of key IDs, not key payloads
 * - Use keyctl_describe() or keyctl_read() on individual IDs
 *   to get more information
 */
static void list_keyring(key_serial_t keyring, const char *name)
{
	char buffer[16384];
	key_serial_t *keys;
	long ret, i, count;

	printf("Listing keyring: %s (ID: %d)\n", name, keyring);

	/*
	 * Reading a keyring returns an array of key IDs
	 * Not the same as reading a regular key (which returns payload)
	 *
	 * The kernel serializes the keyring as:
	 * - Array of key_serial_t (uint32_t)
	 * - Number of entries = ret / sizeof(key_serial_t)
	 */
	ret = keyctl_read(keyring, buffer, sizeof(buffer));
	if (ret < 0) {
		perror("keyctl_read keyring");
		return;
	}

	/*
	 * Cast buffer to array of key IDs
	 * Each entry is a 4-byte key serial number
	 */
	keys = (key_serial_t *)buffer;
	count = ret / sizeof(key_serial_t);

	printf("  Contains %ld keys:\n", count);
	for (i = 0; i < count; i++) {
		printf("    Key ID: %d\n", keys[i]);
	}
	printf("\n");
}

/*
 * show_session_keyring - Demonstrate session keyring operations
 */
static void show_session_keyring(void)
{
	key_serial_t session, key;
	
	printf("=== Session Keyring Demo ===\n\n");
	
	/* Get session keyring */
	session = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);
	if (session < 0) {
		perror("keyctl_get_keyring_ID");
		return;
	}
	
	printf("Session keyring ID: %d\n\n", session);
	
	/* Create keys in session keyring */
	key = create_key("user", "password", "mysecret123", session);
	if (key > 0) {
		show_key_info(key);
		read_key(key);
	}

	key = create_key("user", "token", "token_abc_xyz", session);
	if (key > 0) {
		show_key_info(key);
	}

	/* List keyring contents */
	list_keyring(session, "session");

	/* Search for a key */
	key = search_key(session, "user", "password");
	if (key > 0) {
		read_key(key);
	}
}

/*
 * show_process_keyring - Demonstrate process keyring
 */
static void show_process_keyring(void)
{
	key_serial_t process, key;
	
	printf("=== Process Keyring Demo ===\n\n");
	
	/* Get or create process keyring */
	process = keyctl_get_keyring_ID(KEY_SPEC_PROCESS_KEYRING, 1);
	if (process < 0) {
		perror("keyctl_get_keyring_ID");
		return;
	}
	
	printf("Process keyring ID: %d\n\n", process);
	
	/* Create a key in process keyring */
	key = create_key("user", "process_key", "process_data", process);
	if (key > 0) {
		show_key_info(key);
		list_keyring(process, "process");
	}
}

/*
 * show_user_keyring - Demonstrate user keyring
 */
static void show_user_keyring(void)
{
	key_serial_t user, key;
	
	printf("=== User Keyring Demo ===\n\n");
	
	/* Get user keyring */
	user = keyctl_get_keyring_ID(KEY_SPEC_USER_KEYRING, 1);
	if (user < 0) {
		perror("keyctl_get_keyring_ID");
		return;
	}
	
	printf("User keyring ID: %d\n\n", user);
	
	/* Create persistent key */
	key = create_key("user", "persistent_key", "persistent_data", user);
	if (key > 0) {
		show_key_info(key);
		printf("This key persists across process lifetime\n\n");
	}
}

int main(int argc, char *argv[])
{
	printf("=====================================\n");
	printf("  Key Operations Demonstration\n");
	printf("=====================================\n\n");
	
	printf("Process: %s (PID: %d)\n", argv[0], getpid());
	printf("UID: %d, GID: %d\n\n", getuid(), getgid());
	
	/* Demonstrate different keyring types */
	show_session_keyring();
	show_process_keyring();
	show_user_keyring();
	
	printf("=== Cleanup ===\n\n");
	printf("Keys created in this demo will expire or can be removed with:\n");
	printf("  keyctl show      # List all keys\n");
	printf("  keyctl revoke <key_id>\n");
	printf("  keyctl unlink <key_id> @s\n\n");
	
	printf("View kernel keyring info:\n");
	printf("  cat /proc/keyring\n\n");
	
	return 0;
}

