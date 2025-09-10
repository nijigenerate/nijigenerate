module nijigenerate.api.mcp.https;

import std.exception : enforce;
import std.string : toStringz;

import deimos.openssl.bio;
import deimos.openssl.err;
import deimos.openssl.opensslv;
import deimos.openssl.rand;
import deimos.openssl.ssl;
import deimos.openssl.stack;
import deimos.openssl.x509v3;
import deimos.openssl.pem;

struct SelfSignedCertificate {
    EVP_PKEY* pkey = null;
    X509* x509 = null;
    EC_KEY* ecKey = null;
    ~this() {
        if (pkey !is null) EVP_PKEY_free(pkey);
        if (x509 !is null) X509_free(x509);
        // ecKey is owned by pkey after successful EVP_PKEY_assign_EC_KEY
    }
}

public:
void ngCreateSelfSignedCertificate(string certPath, string keyPath) {
    SelfSignedCertificate cert;

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    RAND_poll();

    // Only generate artifacts; do not configure SSL_CTX here

    // Generate ECC P-256 private key
    cert.pkey = EVP_PKEY_new();
    enforce(cert.pkey !is null, "EVP_PKEY_new failed");
    cert.ecKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    enforce(cert.ecKey !is null, "EC_KEY_new_by_curve_name failed");
    if (EC_KEY_generate_key(cert.ecKey) != 1) {
        EC_KEY_free(cert.ecKey); cert.ecKey = null;
        enforce(false, "EC_KEY_generate_key failed");
    }
    // On failure, EVP_PKEY does not take ownership; free ecKey to avoid leak
    if (EVP_PKEY_assign_EC_KEY(cert.pkey, cert.ecKey) != 1) {
        EC_KEY_free(cert.ecKey);
        cert.ecKey = null;
        enforce(false, "EVP_PKEY_assign_EC_KEY failed");
    }

    // Generate X509 self-signed certificate
    cert.x509 = X509_new();
    enforce(cert.x509 !is null, "X509_new failed");
    ASN1_INTEGER_set(X509_get_serialNumber(cert.x509), 1);
    X509_gmtime_adj(X509_get_notBefore(cert.x509), 0);
    X509_gmtime_adj(X509_get_notAfter(cert.x509), 31536000L); // 1 year validity
    X509_set_pubkey(cert.x509, cert.pkey);

    auto name = X509_get_subject_name(cert.x509);
    X509_NAME_add_entry_by_txt(name, toStringz("C"),  MBSTRING_ASC, cast(ubyte*)toStringz("US"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, toStringz("O"),  MBSTRING_ASC, cast(ubyte*)toStringz("TestOrg"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, toStringz("CN"), MBSTRING_ASC, cast(ubyte*)toStringz("localhost"), -1, -1, 0);

    // Subject Alternative Name: localhost, 127.0.0.1
    auto san = X509V3_EXT_conf_nid(null, null, NID_subject_alt_name, toStringz("DNS:localhost,IP:127.0.0.1"));
    if (san !is null) {
        X509_add_ext(cert.x509, san, -1);
        X509_EXTENSION_free(san);
    }
    X509_set_issuer_name(cert.x509, name);

    enforce(X509_sign(cert.x509, cert.pkey, EVP_sha256()) > 0, "X509_sign failed");

    // Write certificate and private key using OpenSSL BIO to avoid CRT/openssl_uplink issues
    BIO* bioCert = BIO_new_file(toStringz(certPath), toStringz("w"));
    enforce(bioCert !is null, "BIO_new_file for cert failed");
    scope(exit) BIO_free_all(bioCert);
    enforce(PEM_write_bio_X509(bioCert, cert.x509) == 1, "PEM_write_bio_X509 failed");

    BIO* bioKey = BIO_new_file(toStringz(keyPath), toStringz("w"));
    enforce(bioKey !is null, "BIO_new_file for key failed");
    scope(exit) BIO_free_all(bioKey);
    enforce(PEM_write_bio_PrivateKey(bioKey, cert.pkey, null, null, 0, null, null) == 1, "PEM_write_bio_PrivateKey failed");

    // RAII frees cert members automatically
    return;
}