/**
 * otpauth.h
 * OTP Auth URI parser and serializer (Google Authenticator Key Uri Format)
 *
 * STB-style single-header library.
 *
 * Reference: https://github.com/google/google-authenticator/wiki/Key-Uri-Format
 * License: MIT
 */

#ifndef OTPAUTH_H_INCLUDED
#define OTPAUTH_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Public structures --------------------------------------------------- */

typedef struct {
    char *type;           /* "totp" or "hotp" */
    char *issuer;         /* Issuer parameter (may be NULL) */
    char *account;        /* Account name parsed from label */
    char *secret_b32;     /* Base32 encoded secret (required) */
    char *algorithm;      /* HMAC algorithm, default "SHA1" */
    int   digits;         /* Number of digits, default 6 */
    int   counter;        /* Only for HOTP, initial counter value, default 0 */
    int   period;         /* Only for TOTP, time step in seconds, default 30 */
} OTPAuthEntry;

/**
 * Parse an otpauth:// URI.
 *
 * @param uri The URI string (e.g., "otpauth://totp/...")
 * @param out Pointer to OTPAuthEntry structure to be filled.
 * @return true on success, false on failure.
 */
bool otpauth_parse(const char *uri, OTPAuthEntry *out);

/**
 * Serialize an OTPAuthEntry back to an otpauth:// URI.
 *
 * @param entry Pointer to OTPAuthEntry structure.
 * @param buf Output buffer to write URI.
 * @param buf_size Size of output buffer.
 * @return true on success, false if buffer is too small.
 */
bool otpauth_serialize(const OTPAuthEntry *entry, char *buf, size_t buf_size);

/**
 * Free memory allocated inside OTPAuthEntry.
 *
 * @param entry Pointer to OTPAuthEntry structure.
 */
void otpauth_free(OTPAuthEntry *entry);

/**
 * Initialize an OTPAuthEntry structure to all zeros.
 *
 * @param entry Pointer to OTPAuthEntry structure.
 */
void otpauth_init(OTPAuthEntry *entry);

/**
 * Calculate the required buffer size for serialization.
 *
 * @param entry Pointer to OTPAuthEntry structure.
 * @return Required buffer size including null terminator, or 0 on error.
 */
size_t otpauth_serialize_len(const OTPAuthEntry *entry);

#ifdef __cplusplus
}
#endif

/* ---- Implementation ------------------------------------------------------ */
#ifdef OTPAUTH_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

// strcasecmp strncasecmp is not available on Windows, so we define them here for compatibility.
#if defined(_WIN32) || defined(_WIN64)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

/*-----------------------------------------------------------------------------
 * Internal utilities
 *-----------------------------------------------------------------------------*/

/* Decode percent-encoded string (null-terminated) */
static char *otp_url_decode(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (!dst) return NULL;

    char *p = dst;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = { src[1], src[2], 0 };
            char *end;
            unsigned long val = strtoul(hex, &end, 16);
            if (end == hex + 2) {
                *p++ = (char)val;
                src += 3;
                continue;
            }
        }
        *p++ = *src++;
    }
    *p = '\0';
    return dst;
}

static char *otp_strdup(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (dst) memcpy(dst, src, len + 1);
    return dst;
}

static void otp_str_toupper(char *s) {
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

static char *otp_strndup(const char *src, size_t n) {
    char *dst = (char *)malloc(n + 1);
    if (!dst) return NULL;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return dst;
}

/* Parse a single query parameter from a string.
   After a successful parse, *params is advanced past the parsed parameter. */
static bool otp_parse_query_param(char **params, const char *name, char **value) {
    if (!params || !name || !value) return false;
    const char *p = *params;
    if (!p || !*p) return false;
    size_t name_len = strlen(name);

    while (*p) {
        const char *amp = strchr(p, '&');
        size_t pair_len = amp ? (size_t)(amp - p) : strlen(p);
        const char *eq = (const char *)memchr(p, '=', pair_len);
        
        if (eq) {
            size_t key_len = (size_t)(eq - p);
            if (key_len == name_len && strncasecmp(p, name, key_len) == 0) {
                size_t val_len = pair_len - key_len - 1;
                *value = otp_strndup(eq + 1, val_len);
                if (!*value) return false;
                *params = (char *)(amp ? amp + 1 : p + pair_len);
                return true;
            }
        }
        p = amp ? amp + 1 : p + pair_len;
    }
    return false;
}

static bool otp_is_valid_algorithm(const char *algo) {
    if (!algo) return false;
    if (strcasecmp(algo, "SHA1") == 0) return true;
    if (strcasecmp(algo, "SHA256") == 0) return true;
    if (strcasecmp(algo, "SHA512") == 0) return true;
    return false;
}

/*-----------------------------------------------------------------------------
 * URL encoding (returns number of characters written, not including '\0')
 *-----------------------------------------------------------------------------*/
static size_t otp_url_encode(char *dst, const char *src) {
    if (!dst || !src) return 0;
    char *start = dst;
    while (*src) {
        if ((*src >= 'A' && *src <= 'Z') ||
            (*src >= 'a' && *src <= 'z') ||
            (*src >= '0' && *src <= '9') ||
            *src == '-' || *src == '_' || *src == '.' || *src == '~') {
            *dst++ = *src;
        } else {
            sprintf(dst, "%%%02X", (unsigned char)*src);
            dst += 3;
        }
        src++;
    }
    *dst = '\0';
    return (size_t)(dst - start);
}

/*-----------------------------------------------------------------------------
 * Public API implementation
 *-----------------------------------------------------------------------------*/

bool otpauth_parse(const char *uri, OTPAuthEntry *out) {
    const char *p = NULL;
    const char *slash = NULL;
    const char *qmark = NULL;
    size_t label_len = 0;
    char *label_raw = NULL;
    char *label_decoded = NULL;
    const char *colon = NULL;
    const char *query = NULL;
    char *query_copy = NULL;
    char *params_ptr = NULL;
    char *secret_val = NULL;
    char *issuer_val = NULL;
    char *algo_val = NULL;
    char *digits_val = NULL;
    char *counter_val = NULL;
    char *period_val = NULL;
    
    if (!uri || !out) return false;
    memset(out, 0, sizeof(OTPAuthEntry));

    /* Verify scheme */
    if (strncasecmp(uri, "otpauth://", 10) != 0) goto err;
    p = uri + 10;

    /* Parse type (totp/hotp) */
    slash = strchr(p, '/');
    if (!slash) goto err;
    out->type = otp_strndup(p, (size_t)(slash - p));
    if (!out->type) goto err;
    p = slash + 1;

    /* Parse label (account and optional issuer) */
    qmark = strchr(p, '?');
    label_len = qmark ? (size_t)(qmark - p) : strlen(p);
    
    /* Copy label part only, then decode it */
    label_raw = otp_strndup(p, label_len);
    if (!label_raw) goto err;
    label_decoded = otp_url_decode(label_raw);
    free(label_raw);
    label_raw = NULL;  /* Prevent double free in err handler */
    if (!label_decoded) goto err;

    colon = strchr(label_decoded, ':');
    if (colon) {
        /* issuer prefix present in label */
        out->issuer = otp_strndup(label_decoded, (size_t)(colon - label_decoded));
        out->account = otp_strdup(colon + 1);
        if (!out->issuer || !out->account) {
            free(label_decoded);
            label_decoded = NULL;
            goto err;
        }
    } else {
        out->account = otp_strdup(label_decoded);
        if (!out->account) {
            free(label_decoded);
            label_decoded = NULL;
            goto err;
        }
    }
    free(label_decoded);
    label_decoded = NULL;  /* Prevent double free in err handler */

    if (!qmark) {
        /* No query string, but secret is required */
        goto err;
    }
    query = qmark + 1;

    /* Set defaults */
    out->algorithm = otp_strdup("SHA1");
    out->digits = 6;
    out->period = 30;
    out->counter = 0;
    if (!out->algorithm) goto err;

    /* Parse query parameters */
    query_copy = otp_strdup(query);
    if (!query_copy) goto err;
    params_ptr = query_copy;

    if (!otp_parse_query_param(&params_ptr, "secret", &secret_val) || !secret_val) {
        goto err;
    }
    out->secret_b32 = secret_val;
    secret_val = NULL;  /* Ownership transferred */
    otp_str_toupper(out->secret_b32);

    /* issuer if label doesn't contain it */
    if (otp_parse_query_param(&params_ptr, "issuer", &issuer_val)) {
        if (!out->issuer) {
            out->issuer = otp_url_decode(issuer_val);
            free(issuer_val);
            issuer_val = NULL;
        } else {
            free(issuer_val);
            issuer_val = NULL;
        }
    }
    
    /* optional algorithm */
    if (otp_parse_query_param(&params_ptr, "algorithm", &algo_val)) {
        if (otp_is_valid_algorithm(algo_val)) {
            free(out->algorithm);
            out->algorithm = algo_val;
            algo_val = NULL;  /* Ownership transferred */
            otp_str_toupper(out->algorithm);
        } else {
            free(algo_val);
            algo_val = NULL;
        }
    }

    /* digits */
    if (otp_parse_query_param(&params_ptr, "digits", &digits_val) && digits_val) {
        long val = strtol(digits_val, NULL, 10);
        if (val > 0) out->digits = (int)val;
        free(digits_val);
        digits_val = NULL;
    }

    if (out->type && strcasecmp(out->type, "hotp") == 0) {
        if (otp_parse_query_param(&params_ptr, "counter", &counter_val) && counter_val) {
            long val = strtol(counter_val, NULL, 10);
            if (val >= 0) out->counter = (int)val;
            free(counter_val);
            counter_val = NULL;
        }
    } else {
        if (otp_parse_query_param(&params_ptr, "period", &period_val) && period_val) {
            long val = strtol(period_val, NULL, 10);
            if (val > 0) out->period = (int)val;
            free(period_val);
            period_val = NULL;
        }
    }

    /* Success - free temporary resources */
    free(query_copy);
    return true;

err:
    free(query_copy);
    free(label_raw);
    free(label_decoded);
    free(secret_val);
    free(issuer_val);
    free(algo_val);
    free(digits_val);
    free(counter_val);
    free(period_val);
    otpauth_free(out);
    return false;
}


static size_t otp_url_encode_len(const char *s) {
    if (!s) return 0;
    size_t len = 0;
    for (; *s; ++s) {
        if ((*s >= 'A' && *s <= 'Z') ||
            (*s >= 'a' && *s <= 'z') ||
            (*s >= '0' && *s <= '9') ||
            *s == '-' || *s == '_' || *s == '.' || *s == '~') {
            len++;
        } else {
            len += 3; /* %XX */
        }
    }
    return len;
}

size_t otpauth_serialize_len(const OTPAuthEntry *entry) {
    if (!entry || !entry->type || !entry->secret_b32) return 0;

    size_t total = 0;
    total += 10; /* "otpauth://" */
    total += strlen(entry->type);
    total += 1; /* '/' */

    if (entry->issuer) {
        total += otp_url_encode_len(entry->issuer);
        total += 1; /* ':' */
    }
    if (entry->account) {
        total += otp_url_encode_len(entry->account);
    } else if (!entry->issuer) {
        return 0; /* Need account */
    }

    total += 1; /* '?' */
    total += 7 + otp_url_encode_len(entry->secret_b32); /* "secret=" */
    if (entry->algorithm && strcmp(entry->algorithm, "SHA1") != 0) {
        total += 10 + strlen(entry->algorithm); /* "&algorithm=" */
    }
    if (entry->digits != 6) {
        total += 8 + snprintf(NULL, 0, "%d", entry->digits);
    }
    if (entry->type && strcasecmp(entry->type, "hotp") == 0) {
        total += 9 + snprintf(NULL, 0, "%d", entry->counter);
    } else if (entry->period != 30) {
        total += 8 + snprintf(NULL, 0, "%d", entry->period);
    }

    return total + 1; /* +1 for null terminator */
}

bool otpauth_serialize(const OTPAuthEntry *entry, char *buf, size_t buf_size) {
    if (!entry || !entry->type || !entry->secret_b32) return false;
    if (!buf || buf_size == 0) return false;

    size_t total = 0;
    total += 10; /* "otpauth://" */
    total += strlen(entry->type);
    total += 1; /* '/' */

    if (entry->issuer) {
        total += otp_url_encode_len(entry->issuer);
        total += 1; /* ':' */
    }
    if (entry->account) {
        total += otp_url_encode_len(entry->account);
    } else if (!entry->issuer) {
        return false; /* Need account */
    }

    total += 1; /* '?' */
    total += 7 + otp_url_encode_len(entry->secret_b32); /* "secret=" */
    if (entry->algorithm && strcmp(entry->algorithm, "SHA1") != 0) {
        total += 10 + strlen(entry->algorithm); /* "&algorithm=" */
    }
    if (entry->digits != 6) {
        total += 8 + snprintf(NULL, 0, "%d", entry->digits);
    }
    if (entry->type && strcasecmp(entry->type, "hotp") == 0) {
        total += 9 + snprintf(NULL, 0, "%d", entry->counter);
    } else if (entry->period != 30) {
        total += 8 + snprintf(NULL, 0, "%d", entry->period);
    }

    size_t required = otpauth_serialize_len(entry);
    if (required == 0 || buf_size < required) return false;

    char *p = buf;
    p += sprintf(p, "otpauth://%s/", entry->type);
    if (entry->issuer) {
        p += otp_url_encode(p, entry->issuer);
        *p++ = ':';
    }
    if (entry->account) {
        p += otp_url_encode(p, entry->account);
    }
    *p++ = '?';
    p += sprintf(p, "secret=");
    p += otp_url_encode(p, entry->secret_b32);
    if (entry->algorithm && strcmp(entry->algorithm, "SHA1") != 0) {
        p += sprintf(p, "&algorithm=%s", entry->algorithm);
    }
    if (entry->digits != 6) {
        p += sprintf(p, "&digits=%d", entry->digits);
    }
    if (entry->type && strcasecmp(entry->type, "hotp") == 0) {
        p += sprintf(p, "&counter=%d", entry->counter);
    } else if (entry->period != 30) {
        p += sprintf(p, "&period=%d", entry->period);
    }
    *p = '\0';
    return true;
}

void otpauth_free(OTPAuthEntry *entry) {
    if (!entry) return;
    free(entry->type);
    free(entry->issuer);
    free(entry->account);
    free(entry->secret_b32);
    free(entry->algorithm);
    memset(entry, 0, sizeof(OTPAuthEntry));
}

void otpauth_init(OTPAuthEntry *entry) {
    if (entry) memset(entry, 0, sizeof(OTPAuthEntry));
}

#endif /* OTPAUTH_IMPLEMENTATION */
#endif /* OTPAUTH_H_INCLUDED */