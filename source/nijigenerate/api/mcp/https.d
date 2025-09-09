module nijigenerate.api.mcp.https;

import core.stdc.stdio : FILE, fopen, fclose, stderr;
import core.stdc.stdlib : exit, EXIT_FAILURE;
import std.stdio;

import deimos.openssl.bio;
import deimos.openssl.err;
import deimos.openssl.opensslv;
import deimos.openssl.rand;
import deimos.openssl.ssl;
import deimos.openssl.stack;
import deimos.openssl.x509v3;

public:

extern(C) SSL_CTX* ngCreateSelfSignedCertificate() {
    SSL_CTX* ctx;
    EVP_PKEY* pkey = null;
    X509* x509 = null;

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ctx = SSL_CTX_new(TLS_server_method());
    if (ctx is null) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Set TLS 1.2+ only and disable insecure options
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);

    // Generate ECC P-256 private key
    pkey = EVP_PKEY_new();
    auto ecKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (EC_KEY_generate_key(ecKey) != 1) {
        writefln("Failed to generate ECC key\n");
        exit(EXIT_FAILURE);
    }
    EVP_PKEY_assign_EC_KEY(pkey, ecKey);

    // Generate X509 self-signed certificate
    x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L); // 1 year validity
    X509_set_pubkey(x509, pkey);

    auto name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, cast(ubyte*)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, cast(ubyte*)"TestOrg", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, cast(ubyte*)"localhost", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    X509_sign(x509, pkey, EVP_sha256());

    // Write certificate and private key to files (vibe-d requires file)
    auto certFile = fopen("server.crt", "w");
    if (certFile is null) {
        writefln("Failed to open server.crt\n");
        exit(EXIT_FAILURE);
    }
    PEM_write_X509(certFile, x509);
    fclose(certFile);

    auto keyFile = fopen("server.key", "w");
    if (keyFile is null) {
        writefln("Failed to open server.key\n");
        exit(EXIT_FAILURE);
    }
    PEM_write_PrivateKey(keyFile, pkey, null, null, 0, null, null);
    fclose(keyFile);

    // Set certificate and key to SSL_CTX
    if (SSL_CTX_use_certificate(ctx, x509) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey(ctx, pkey) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    EVP_PKEY_free(pkey);
    X509_free(x509);

    return ctx;
}