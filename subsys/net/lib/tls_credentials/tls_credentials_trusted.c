/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <psa/protected_storage.h>

#include "tls_internal.h"
#include "tls_credentials_digest_raw.h"

/* This lets check which MbedTLS features are enabled */
#include "tls_credentials_mbedtls_config.h"

LOG_MODULE_REGISTER(tls_credentials_trusted,
		    CONFIG_TLS_CREDENTIALS_LOG_LEVEL);

/* This implementation uses the PSA Protected Storage API to store:
 * - credentials with an UID constructed as:
 *	[ C2E0 ] | [ type as uint16_t ] | [ tag as uint32_t ]
 * - credential ToC with an UID constructed as:
 *	[ C2E0 ] | [ ffff as uint16_t ] | [ ffffffff as uint32_t ]
 *
 * The ToC contains a list of CONFIG_TLS_MAX_CREDENTIALS_NUMBER UIDs
 * of credentials, can be 0 if slot is free.
 */

#define PSA_PS_CRED_ID		0xC2E0ULL

#define CRED_MAX_SLOTS	CONFIG_TLS_MAX_CREDENTIALS_NUMBER

/* Global temporary pool of credentials to be used by TLS contexts. */
static struct tls_credential credentials[CRED_MAX_SLOTS];

/* Credentials Table Of Content copy of the one stored in Protected Storage */
static psa_storage_uid_t credentials_toc[CRED_MAX_SLOTS];

/* A mutex for protecting access to the credentials array. */
static struct k_mutex credential_lock;

/* Construct PSA PS uid from tag & type */
static inline psa_storage_uid_t tls_credential_get_uid(uint32_t tag,
						       uint16_t type)
{
	return PSA_PS_CRED_ID << 48 |
	       (type & 0xffffULL) << 32 |
	       (tag & 0xffffffff);
}

#define PSA_PS_CRED_TOC_ID tls_credential_get_uid(0xffffffff, 0xffff)

/* Get the TAG from an UID */
static inline sec_tag_t tls_credential_uid_to_tag(psa_storage_uid_t uid)
{
	return (uid & 0xffffffff);
}

/* Get the TYPE from an UID */
static inline int tls_credential_uid_to_type(psa_storage_uid_t uid)
{
	return ((uid >> 32) & 0xffff);
}

static int credentials_toc_get(void)
{
	psa_status_t status;
	size_t len;

	status = psa_ps_get(PSA_PS_CRED_TOC_ID, 0, sizeof(credentials_toc),
			    credentials_toc, &len);
	if (status == PSA_ERROR_DOES_NOT_EXIST) {
		return -ENOENT;
	} else if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

static int credentials_toc_write(void)
{
	psa_status_t status;

	status = psa_ps_set(PSA_PS_CRED_TOC_ID, sizeof(credentials_toc),
			    credentials_toc, 0);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

static int credentials_toc_update(unsigned int slot, psa_storage_uid_t uid)
{
	int ret;

	if (slot >= CRED_MAX_SLOTS) {
		return -EINVAL;
	}

	credentials_toc[slot] = uid;

	ret = credentials_toc_write();
	if (ret) {
		return ret;
	}

	return credentials_toc_get();
}

static unsigned int tls_credential_toc_find_slot(psa_storage_uid_t uid)
{
	unsigned int slot;

	for (slot = 0; slot < CRED_MAX_SLOTS; ++slot) {
		if (credentials_toc[slot] == uid) {
			return slot;
		}
	}

	return CRED_MAX_SLOTS;
}

static int credentials_init(void)
{
	struct psa_storage_info_t info;
	unsigned int sync = 0;
	psa_status_t status;
	unsigned int slot;
	int ret;

	/* Retrieve Table of Content from storage */
	ret = credentials_toc_get();
	if (ret == -ENOENT) {
		memset(credentials_toc, 0, sizeof(credentials_toc));
		return 0;
	} else if (ret != 0) {
		return -EIO;
	}

	/* Check validity of ToC */
	for (slot = 0; slot < CRED_MAX_SLOTS; ++slot) {
		if (credentials_toc[slot] == 0) {
			continue;
		}

		status = psa_ps_get_info(credentials_toc[slot], &info);
		if (status == PSA_ERROR_DOES_NOT_EXIST) {
			LOG_WRN("Credential %d doesn't exist in storage", slot);
			credentials_toc[slot] = 0;
			sync = 1;
		} else if (status != PSA_SUCCESS) {
			return -EIO;
		}
	}

	if (sync != 0) {
		ret = credentials_toc_write();
		if (ret != 0) {
			return -EIO;
		}
	}

	return 0;
}
SYS_INIT(credentials_init, POST_KERNEL, 0);

static struct tls_credential *unused_credential_get(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(credentials); i++) {
		if (credentials[i].type == TLS_CREDENTIAL_NONE) {
			return &credentials[i];
		}
	}

	return NULL;
}

/* Get a credential struct from an UID */
static struct tls_credential *credential_get_from_uid(psa_storage_uid_t uid)
{
	struct tls_credential *credential;
	struct psa_storage_info_t info;
	psa_status_t status;

	if (tls_credential_toc_find_slot(uid) == CRED_MAX_SLOTS) {
		return NULL;
	}

	credential = unused_credential_get();
	if (credential == NULL) {
		return NULL;
	}

	status = psa_ps_get_info(uid, &info);
	if (status != PSA_SUCCESS) {
		return NULL;
	}

	credential->buf = k_malloc(info.size);
	if (credential->buf == NULL) {
		return NULL;
	}

	status = psa_ps_get(uid, 0, info.size, (void *)credential->buf,
			    &credential->len);
	if (status != PSA_SUCCESS) {
		k_free((void *)credential->buf);
		credential->buf = NULL;
		return NULL;
	}

	credential->tag = tls_credential_uid_to_tag(uid);
	credential->type = tls_credential_uid_to_type(uid);

	return credential;
}

/* Get a credential struct from a TAG and TYPE values */
struct tls_credential *credential_get(sec_tag_t tag,
				      enum tls_credential_type type)
{
	return credential_get_from_uid(tls_credential_get_uid(tag, type));
}


/* Get the following credential filtered by a TAG valud */
struct tls_credential *credential_next_get(sec_tag_t tag,
					   struct tls_credential *iter)
{
	psa_storage_uid_t uid;
	unsigned int slot;

	if (!iter) {
		slot = 0;
	} else {
		uid = tls_credential_get_uid(iter->tag, iter->type);

		slot = tls_credential_toc_find_slot(uid);
		if (slot == CRED_MAX_SLOTS) {
			return NULL;
		}

		slot++;
	}

	for (; slot < CRED_MAX_SLOTS; slot++) {
		uid = credentials_toc[slot];
		if (uid == 0) {
			continue;
		}

		if (tls_credential_uid_to_type(uid) != TLS_CREDENTIAL_NONE &&
		    tls_credential_uid_to_tag(uid) == tag) {
			return credential_get_from_uid(uid);
		}
	}

	return NULL;
}

sec_tag_t credential_next_tag_get(sec_tag_t iter)
{
	unsigned int slot;
	psa_storage_uid_t uid;
	sec_tag_t lowest_candidate = TLS_SEC_TAG_NONE;
	sec_tag_t candidate;

	/* Scan all slots and find lowest sectag greater than iter */
	for (slot = 0; slot < CRED_MAX_SLOTS; slot++) {
		uid = credentials_toc[slot];

		/* Skip empty slots. */
		if (uid == 0) {
			continue;
		}
		if (tls_credential_uid_to_type(uid) == TLS_CREDENTIAL_NONE) {
			continue;
		}

		candidate = tls_credential_uid_to_tag(uid);

		/* Skip any slots containing sectags not greater than iter */
		if (candidate <= iter && iter != TLS_SEC_TAG_NONE) {
			continue;
		}

		/* Find the lowest of such slots */
		if (lowest_candidate == TLS_SEC_TAG_NONE || candidate < lowest_candidate) {
			lowest_candidate = candidate;
		}
	}

	return lowest_candidate;
}

int credential_digest(struct tls_credential *credential, void *dest, size_t *len)
{
	return credential_digest_raw(credential, dest, len);
}

void credentials_lock(void)
{
	k_mutex_lock(&credential_lock, K_FOREVER);
}

void credentials_unlock(void)
{
	int i;

	/* Erase & free all retrieved credentials */
	for (i = 0; i < ARRAY_SIZE(credentials); i++) {
		if (credentials[i].buf) {
			k_free((void *)credentials[i].buf);
		}
		memset(&credentials[i], 0, sizeof(credentials[i]));
	}

	k_mutex_unlock(&credential_lock);
}

/* Double check that security tag and credential type are allowed */
static bool tag_type_valid(sec_tag_t tag, enum tls_credential_type type)
{
	/* tag 0xffffffff type 0xffff are reserved */
	return !(tag == 0xffffffff && type == 0xffff);
}

int tls_credential_add(sec_tag_t tag, enum tls_credential_type type,
		       const void *cred, size_t credlen)
{
	psa_storage_uid_t uid = tls_credential_get_uid(tag, type);
	psa_storage_create_flags_t create_flags = 0;
	psa_status_t status;
	unsigned int slot;
	int ret = 0;

	if (!tag_type_valid(tag, type)) {
		ret = -EINVAL;
		goto cleanup;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);

	if (tls_credential_toc_find_slot(uid) != CRED_MAX_SLOTS) {
		ret = -EEXIST;
		goto cleanup;
	}

	slot = tls_credential_toc_find_slot(0);
	if (slot == CRED_MAX_SLOTS) {
		ret = -ENOMEM;
		goto cleanup;
	}

	/* TODO: Set create_flags depending on tag value ? */

	status = psa_ps_set(uid, credlen, cred, create_flags);
	if (status != PSA_SUCCESS) {
		ret = -EIO;
		goto cleanup;
	}

	ret = credentials_toc_update(slot, uid);

cleanup:
	k_mutex_unlock(&credential_lock);

	return ret;
}

int tls_credential_get(sec_tag_t tag, enum tls_credential_type type,
		       void *cred, size_t *credlen)
{
	struct psa_storage_info_t info;
	psa_storage_uid_t uid = tls_credential_get_uid(tag, type);
	psa_status_t status;
	unsigned int slot;
	int ret = 0;

	if (!tag_type_valid(tag, type)) {
		ret = -EINVAL;
		goto cleanup;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);

	slot = tls_credential_toc_find_slot(uid);
	if (slot == CRED_MAX_SLOTS) {
		ret = -ENOENT;
		goto cleanup;
	}

	status = psa_ps_get_info(uid, &info);
	if (status == PSA_ERROR_DOES_NOT_EXIST) {
		ret = -ENOENT;
		goto cleanup;
	} else if (status != PSA_SUCCESS) {
		ret = -EIO;
		goto cleanup;
	}

	if (info.size > *credlen) {
		ret = -EFBIG;
		goto cleanup;
	}

	status = psa_ps_get(uid, 0, info.size, cred, credlen);
	if (status != PSA_SUCCESS) {
		ret = -EIO;
		goto cleanup;
	}

cleanup:
	k_mutex_unlock(&credential_lock);

	return ret;
}

int tls_credential_delete(sec_tag_t tag, enum tls_credential_type type)
{
	psa_storage_uid_t uid = tls_credential_get_uid(tag, type);
	psa_status_t status;
	unsigned int slot;
	int ret = 0;

	if (!tag_type_valid(tag, type)) {
		ret = -EINVAL;
		goto cleanup;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);

	slot = tls_credential_toc_find_slot(uid);
	if (slot == CRED_MAX_SLOTS) {
		ret = -ENOENT;
		goto cleanup;
	}

	ret = credentials_toc_update(slot, 0);
	if (ret != 0) {
		goto cleanup;
	}

	status = psa_ps_remove(uid);
	if (status == PSA_ERROR_DOES_NOT_EXIST) {
		ret = -ENOENT;
		goto cleanup;
	} else if (status != PSA_SUCCESS) {
		ret = -EIO;
		goto cleanup;
	}

cleanup:
	k_mutex_unlock(&credential_lock);

	return ret;
}


#if defined(CONFIG_TLS_CREDENTIAL_CSR)
/* We need MBEDTLS_USE_PSA_CRYPTO because this implementation uses mbedtls_pk_setup_opaque */
#if defined(CONFIG_MBEDTLS) && defined(MBEDTLS_USE_PSA_CRYPTO) && defined(MBEDTLS_X509_CSR_WRITE_C)

#include <psa/crypto.h>
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"

/**
 * @brief PSA Random number generator wrapper for Mbed TLS
 */
static int psa_rng_for_mbedtls(void *p_rng, unsigned char *output, size_t output_len)
{
	ARG_UNUSED(p_rng);

	return psa_generate_random(output, output_len);
}

/* For this backend, the private-key will be stored in DER format.
 * Use `cred get <tag> PK bin` to retrieve (the non-terminated base64 encoding of) the key using
 * the credential shell.
 */
int tls_credential_csr(sec_tag_t tag, char *dn, void *csr, size_t *csr_len)
{
	int ret = 0;

	int cred_status;
	psa_status_t psa_status;
	int mbed_status;

	bool key_created = false;
	bool key_stored = false;

	/* Credential Storage */
	psa_storage_uid_t uid = tls_credential_get_uid(tag, TLS_CREDENTIAL_PRIVATE_KEY);

	/* Keygen */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = PSA_KEY_ID_NULL;
	psa_key_usage_t key_usage;

	/* CSR and key export */
	mbedtls_pk_context pk_ctx;
	mbedtls_x509write_csr writer;
	void* cred_start = 0;
	size_t csr_max = *csr_len;

	/* We haven't written anything yet... */
	*csr_len = 0;

	if (!tag_type_valid(tag, TLS_CREDENTIAL_PRIVATE_KEY)) {
		return -EINVAL;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);

	/* Create temporary contexts */
	mbedtls_pk_init(&pk_ctx);
	mbedtls_x509write_csr_init(&writer);

	/* Verify sectag not already taken */
	if (tls_credential_toc_find_slot(uid) != CRED_MAX_SLOTS) {
		ret = -EEXIST;
		goto cleanup;
	}

	/* Before attempting keygen, check that an empty slot is available */
	if (tls_credential_toc_find_slot(0) == CRED_MAX_SLOTS) {
		ret = -ENOMEM;
		goto cleanup;
	}

	/* Use PSA to generate a volatile SECP256R1 private/public key-pair
         *
	 * It is possible (and better) to mark the private key as persistent, and store it directly
	 * in Trusted Internal Storage. This would be more secure, but until opaque key support is
	 * added, keys will still need to be exported into non-secure memory, largely defeating
	 * the security benefits.
	 *
	 * Accordingly, for now, we simply generate a volatile key and transfer it into
	 * protected storage.
	 */

	key_usage = PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_SIGN_HASH;
	psa_set_key_usage_flags(&key_attributes, key_usage);

	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);

	psa_status = psa_crypto_init();

	if (psa_status != PSA_SUCCESS) {
		LOG_ERR("Failed to initialize crypto. Status: %d", psa_status);
		ret = -EFAULT;
		goto cleanup;
	}

	psa_status = psa_generate_key(&key_attributes, &key_id);

	if (psa_status != PSA_SUCCESS) {
		LOG_ERR("Failed to generate private key for CSR. Status: %d", psa_status);
		ret = -EFAULT;
		goto cleanup;
	}

	/* If PSA key was successfully created, we must later destroy it. */
	key_created = true;

	/* Hand key off to MbedTLS for immediate use (CSR and formatted export). */
	mbed_status = mbedtls_pk_setup_opaque(&pk_ctx, key_id);
	if (mbed_status) {
		LOG_ERR("Failed to set up opaque private key. Status: %d", mbed_status);
		ret = -EFAULT;
		mbedtls_pk_free(&pk_ctx);
		goto cleanup;
	}

	/* Export private key material in the same RFC5915/SEC1 DER format that
	 * MbedTLS will later expect when loading the private key material from storage.
	 *
	 * We will temporarily use the CSR buffer to hold on to the formatted data.
	 */
	mbed_status = mbedtls_pk_write_key_der(&pk_ctx, csr, csr_max);
	if (mbed_status == MBEDTLS_ERR_ASN1_BUF_TOO_SMALL) {
		LOG_ERR("Failed to format private key material. CSR buffer too small.");
		ret = -EFBIG;
		goto cleanup;
	}
	if (mbed_status < 0) {
		LOG_ERR("Failed to format private key material. Status: %d", mbed_status);
		ret = -EFAULT;
		goto cleanup;
	}

	/* Place the formatted private key in credentials storage.
	 * Note that mbedtls_pk_write_key_der writes the key to the end of the csr buffer,
	 * hence the need for cred_head.
	 */
	cred_start = (char*)csr + csr_max - mbed_status;
	cred_status = tls_credential_add(tag, TLS_CREDENTIAL_PRIVATE_KEY, cred_start, mbed_status);
	if (cred_status != 0) {
		LOG_ERR("Error storing CSR private key: %d", cred_status);
		ret = -EFAULT;
		goto cleanup;
	}

	/* Clear CSR buffer afterwards to prevent accidental private key leak */
	memset(csr, 0, csr_max);
	*csr_len = 0;

	/* If key private data was successfully stored, we may need to delete it in the event of an
	 * error during CSR writing.
	 */
	key_stored = true;

	/* Configure CSR writer */
	mbedtls_x509write_csr_set_md_alg(&writer, MBEDTLS_MD_SHA256);
	mbedtls_x509write_csr_set_key(&writer, &pk_ctx);
	mbed_status = mbedtls_x509write_csr_set_subject_name(&writer, dn);
	if (mbed_status) {
		LOG_ERR("Could not set distinguished name for CSR, error %d", mbed_status);
		ret = -EINVAL;
		goto cleanup;
	}

	/* Write CSR to output buffer. */
	mbed_status = mbedtls_x509write_csr_der(&writer, csr, csr_max, psa_rng_for_mbedtls, NULL);

	if (mbed_status == MBEDTLS_ERR_ASN1_BUF_TOO_SMALL) {
		LOG_ERR("Failed to write CSR. Provided buffer too small.");
		ret = -EFBIG;
		goto cleanup;
	}
	if (mbed_status < 0) {
		LOG_ERR("Failed to write CSR. Status: %d", mbed_status);
		ret = -EFAULT;
		goto cleanup;
	}

	/* On success, mbed_status contains the number of bytes written. */
	*csr_len = mbed_status;

	/* mbedtls_x509write_csr_der writes its data to the end of the csr buffer.
	 * Shift this data to the beginning of the csr buffer for convenient use by the caller.
	 */
	cred_start = (char*)csr + csr_max - mbed_status;
	memmove(csr, cred_start, mbed_status);

cleanup:
	/* Destroy temporary contexts */
	mbedtls_x509write_csr_free(&writer);
	mbedtls_pk_free(&pk_ctx);

	/* Unstore private key if an error occurred after it was stored */
	if (ret && key_stored) {
		cred_status = tls_credential_delete(tag, TLS_CREDENTIAL_PRIVATE_KEY);
		if (cred_status) {
			LOG_ERR("Failed to unstore CSR private key: %d ", cred_status);
		}
	}

	/* If an error occurred, the CSR buffer is not valid. Wipe it.*/
	if (ret) {
		memset(csr, 0, csr_max);
		*csr_len = 0;
	}

	/* Destroy temporary PSA key if it was created */
	if (key_created) {
		psa_status = psa_destroy_key(key_id);
		if (psa_status != PSA_SUCCESS) {
			// Reviewers: Should this trigger a panic?
			LOG_ERR("Failed to destroy keypair after CSR generation: %d", psa_status);
		}
	}

	k_mutex_unlock(&credential_lock);

	return ret;
}

#else /* CONFIG_MBEDTLS */

int tls_credential_csr(sec_tag_t tag, char *dn, void *csr, size_t *csr_len)
{
	/* Mbed-TLS is required for CSR generation. */
	return -ENOTSUP;
}

#endif /* CONFIG_MBEDTLS */
#endif /* defined(CONFIG_TLS_CREDENTIAL_CSR) */