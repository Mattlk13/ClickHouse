#include <Access/AccessControl.h>
#include <Access/AuthenticationData.h>
#include <Access/Common/AuthenticationType.h>
#include <Common/Base64.h>
#include <Common/Exception.h>
#include <IO/WriteHelpers.h>
#include <Interpreters/Access/getValidUntilFromAST.h>
#include <Interpreters/Context.h>
#include <Interpreters/evaluateConstantExpression.h>
#include <Parsers/ASTLiteral.h>
#include <Parsers/Access/ASTPublicSSHKey.h>
#include <Storages/checkAndGetLiteralArgument.h>

#include <boost/algorithm/hex.hpp>
#include <Poco/SHA1Engine.h>

#include "config.h"

#if USE_SSL
#    include <openssl/rand.h>
#    include <openssl/err.h>
#    include <Common/Crypto/X509Certificate.h>
#    include <Common/OpenSSLHelpers.h>
#endif

#if USE_BCRYPT
#     include <bcrypt.h>
#endif

namespace DB
{

namespace ErrorCodes
{
    extern const int AUTHENTICATION_FAILED;
    extern const int SUPPORT_IS_DISABLED;
    extern const int BAD_ARGUMENTS;
    extern const int LOGICAL_ERROR;
    extern const int NOT_IMPLEMENTED;
    extern const int OPENSSL_ERROR;
}


AuthenticationData::Digest AuthenticationData::Util::encodeSHA256(std::string_view text [[maybe_unused]])
{
#if USE_SSL
    Digest hash;
    hash.resize(32);
    ::DB::encodeSHA256(text, hash.data());
    return hash;
#else
    throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "SHA256 passwords support is disabled, because ClickHouse was built without SSL library");
#endif
}

AuthenticationData::Digest AuthenticationData::Util::encodeScramSHA256(std::string_view password [[maybe_unused]], std::string_view salt [[maybe_unused]])
{
#if USE_SSL
    std::vector<uint8_t> salt_digest;
    for (auto elem : base64Decode(String(salt)))
        salt_digest.push_back(elem);
    auto salted_password = pbkdf2SHA256(password, salt_digest, 4096);
    return salted_password;
#else
    throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "SCRAM SHA256 passwords support is disabled, because ClickHouse was built without SSL library");
#endif
}


AuthenticationData::Digest AuthenticationData::Util::encodeSHA1(std::string_view text)
{
    Poco::SHA1Engine engine;
    engine.update(text.data(), text.size());
    return engine.digest();
}

AuthenticationData::Digest AuthenticationData::Util::encodeBcrypt(std::string_view text [[maybe_unused]], int workfactor [[maybe_unused]])
{
#if USE_BCRYPT
    if (text.size() > 72)
        throw Exception(
            ErrorCodes::BAD_ARGUMENTS,
            "bcrypt does not support passwords with a length of more than 72 bytes");

    char salt[BCRYPT_HASHSIZE];
    Digest hash;
    hash.resize(64);

    int ret = bcrypt_gensalt(workfactor, salt);
    if (ret != 0)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "BCrypt library failed: bcrypt_gensalt returned {}", ret);

    ret = bcrypt_hashpw(text.data(), salt, reinterpret_cast<char *>(hash.data()));  /// NOLINT(bugprone-suspicious-stringview-data-usage)
    if (ret != 0)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "BCrypt library failed: bcrypt_hashpw returned {}", ret);

    return hash;
#else
    throw Exception(
        ErrorCodes::SUPPORT_IS_DISABLED,
        "bcrypt passwords support is disabled, because ClickHouse was built without bcrypt library");
#endif
}

bool AuthenticationData::Util::checkPasswordBcrypt(std::string_view password [[maybe_unused]], const Digest & password_bcrypt [[maybe_unused]])
{
#if USE_BCRYPT
    int ret = bcrypt_checkpw(password.data(), reinterpret_cast<const char *>(password_bcrypt.data()));  /// NOLINT(bugprone-suspicious-stringview-data-usage)
    /// Before 24.6 we didn't validate hashes on creation, so it could be that the stored hash is invalid
    /// and it could not be decoded by the library
    if (ret == -1)
        throw Exception(ErrorCodes::AUTHENTICATION_FAILED, "Internal failure decoding Bcrypt hash");
    return (ret == 0);
#else
    throw Exception(
        ErrorCodes::SUPPORT_IS_DISABLED,
        "bcrypt passwords support is disabled, because ClickHouse was built without bcrypt library");
#endif
}

bool operator ==(const AuthenticationData & lhs, const AuthenticationData & rhs)
{
    return (lhs.type == rhs.type) && (lhs.password_hash == rhs.password_hash)
        && (lhs.ldap_server_name == rhs.ldap_server_name) && (lhs.kerberos_realm == rhs.kerberos_realm)
#if USE_SSL
        && (lhs.ssl_certificate_subjects == rhs.ssl_certificate_subjects)
#endif
#if USE_SSH
        && (lhs.ssh_keys == rhs.ssh_keys)
#endif
        && (lhs.http_auth_scheme == rhs.http_auth_scheme)
        && (lhs.http_auth_server_name == rhs.http_auth_server_name)
        && (lhs.valid_until == rhs.valid_until);
}


void AuthenticationData::setPassword(const String & password_, bool validate)
{
    switch (type)
    {
        case AuthenticationType::PLAINTEXT_PASSWORD:
            setPasswordHashBinary(Util::stringToDigest(password_), validate);
            return;

        case AuthenticationType::SHA256_PASSWORD:
            setPasswordHashBinary(Util::encodeSHA256(password_), validate);
            return;

        case AuthenticationType::SCRAM_SHA256_PASSWORD:
            setPasswordHashBinary(Util::encodeScramSHA256(password_, ""), validate);
            return;

        case AuthenticationType::DOUBLE_SHA1_PASSWORD:
            setPasswordHashBinary(Util::encodeDoubleSHA1(password_), validate);
            return;

        case AuthenticationType::BCRYPT_PASSWORD:
        case AuthenticationType::NO_PASSWORD:
        case AuthenticationType::LDAP:
        case AuthenticationType::JWT:
        case AuthenticationType::KERBEROS:
        case AuthenticationType::SSL_CERTIFICATE:
        case AuthenticationType::SSH_KEY:
        case AuthenticationType::HTTP:
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot specify password for authentication type {}", toString(type));

        case AuthenticationType::MAX:
            break;
    }
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "setPassword(): authentication type {} not supported", toString(type));
}

void AuthenticationData::setPasswordBcrypt(const String & password_, int workfactor_, bool validate)
{
    if (type != AuthenticationType::BCRYPT_PASSWORD)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot specify bcrypt password for authentication type {}", toString(type));

    setPasswordHashBinary(Util::encodeBcrypt(password_, workfactor_), validate);
}

String AuthenticationData::getPassword() const
{
    if (type != AuthenticationType::PLAINTEXT_PASSWORD)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot decode the password");
    return String(password_hash.data(), password_hash.data() + password_hash.size());
}


void AuthenticationData::setPasswordHashHex(const String & hash, bool validate)
{
    Digest digest;
    digest.resize(hash.size() / 2);

    try
    {
        boost::algorithm::unhex(hash.begin(), hash.end(), digest.data());
    }
    catch (const std::exception &)
    {
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "Cannot read password hash in hex, check for valid characters [0-9a-fA-F] and length");
    }

    setPasswordHashBinary(digest, validate);
}


String AuthenticationData::getPasswordHashHex() const
{
    if (type == AuthenticationType::LDAP || type == AuthenticationType::KERBEROS || type == AuthenticationType::SSL_CERTIFICATE)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot get password hex hash for authentication type {}", toString(type));

    String hex;
    hex.resize(password_hash.size() * 2);
    boost::algorithm::hex(password_hash.begin(), password_hash.end(), hex.data());
    return hex;
}


void AuthenticationData::setPasswordHashBinary(const Digest & hash, bool validate)
{
    switch (type)
    {
        case AuthenticationType::PLAINTEXT_PASSWORD:
        {
            password_hash = hash;
            return;
        }

        case AuthenticationType::SHA256_PASSWORD:
        {
            if (hash.size() != 32)
                throw Exception(ErrorCodes::BAD_ARGUMENTS,
                                "Password hash for the 'SHA256_PASSWORD' authentication type has length {} "
                                "but must be exactly 32 bytes.", hash.size());
            password_hash = hash;
            return;
        }

        case AuthenticationType::SCRAM_SHA256_PASSWORD:
        {
            password_hash = hash;
            return;
        }

        case AuthenticationType::DOUBLE_SHA1_PASSWORD:
        {
            if (validate && hash.size() != 20)
                throw Exception(ErrorCodes::BAD_ARGUMENTS,
                                "Password hash for the 'DOUBLE_SHA1_PASSWORD' authentication type has length {} "
                                "but must be exactly 20 bytes.", hash.size());
            password_hash = hash;
            return;
        }

        case AuthenticationType::BCRYPT_PASSWORD:
        {
            /// Depending on the workfactor the resulting hash can be 59 or 60 characters long.
            /// However the library we use to encode it requires hash string to be 64 characters long,
            ///  so we also allow the hash of this length.

            if (validate && hash.size() != 59 && hash.size() != 60 && hash.size() != 64)
                throw Exception(ErrorCodes::BAD_ARGUMENTS,
                                "Password hash for the 'BCRYPT_PASSWORD' authentication type has length {} "
                                "but must be 59 or 60 bytes.", hash.size());

            auto resized = hash;
            resized.resize(64);

#if USE_BCRYPT
            if (validate)
            {
                /// Verify that it is a valid hash
                int ret = bcrypt_checkpw("", reinterpret_cast<const char *>(resized.data()));
                if (ret == -1)
                    throw Exception(ErrorCodes::BAD_ARGUMENTS, "Could not decode the provided hash with 'bcrypt_hash'");
            }
#endif

            password_hash = hash;
            password_hash.resize(64);
            return;
        }

        case AuthenticationType::NO_PASSWORD:
        case AuthenticationType::LDAP:
        case AuthenticationType::JWT:
        case AuthenticationType::KERBEROS:
        case AuthenticationType::SSL_CERTIFICATE:
        case AuthenticationType::SSH_KEY:
        case AuthenticationType::HTTP:
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot specify password binary hash for authentication type {}", toString(type));

        case AuthenticationType::MAX:
            break;
    }
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "setPasswordHashBinary(): authentication type {} not supported", toString(type));
}

void AuthenticationData::setSalt(String salt_)
{
    if (type != AuthenticationType::SHA256_PASSWORD && type != AuthenticationType::SCRAM_SHA256_PASSWORD)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "setSalt(): authentication type {} not supported", toString(type));
    salt = std::move(salt_);
}

String AuthenticationData::getSalt() const
{
    return salt;
}

#if USE_SSL
void AuthenticationData::setSSLCertificateSubjects(X509Certificate::Subjects && ssl_certificate_subjects_)
{
    if (ssl_certificate_subjects_.empty())
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "The 'SSL CERTIFICATE' authentication type requires a non-empty list of subjects.");
    ssl_certificate_subjects = std::move(ssl_certificate_subjects_);
}

void AuthenticationData::addSSLCertificateSubject(X509Certificate::Subjects::Type type_, String && subject_)
{
    ssl_certificate_subjects.insert(type_, std::move(subject_));
}
#endif

std::shared_ptr<ASTAuthenticationData> AuthenticationData::toAST() const
{
    auto node = std::make_shared<ASTAuthenticationData>();
    auto auth_type = getType();
    node->type = auth_type;

    switch (auth_type)
    {
        case AuthenticationType::PLAINTEXT_PASSWORD:
        {
            node->contains_password = true;
            node->children.push_back(std::make_shared<ASTLiteral>(getPassword()));
            break;
        }
        case AuthenticationType::SHA256_PASSWORD:
        {
            node->contains_hash = true;
            node->children.push_back(std::make_shared<ASTLiteral>(getPasswordHashHex()));

            if (!getSalt().empty())
                node->children.push_back(std::make_shared<ASTLiteral>(getSalt()));
            break;
        }
        case AuthenticationType::SCRAM_SHA256_PASSWORD:
        {
            node->contains_hash = true;
            node->children.push_back(std::make_shared<ASTLiteral>(getPasswordHashHex()));

            if (!getSalt().empty())
                node->children.push_back(std::make_shared<ASTLiteral>(getSalt()));
            break;
        }
        case AuthenticationType::DOUBLE_SHA1_PASSWORD:
        {
            node->contains_hash = true;
            node->children.push_back(std::make_shared<ASTLiteral>(getPasswordHashHex()));
            break;
        }
        case AuthenticationType::BCRYPT_PASSWORD:
        {
            node->contains_hash = true;
            node->children.push_back(std::make_shared<ASTLiteral>(AuthenticationData::Util::digestToString(getPasswordHashBinary())));
            break;
        }
        case AuthenticationType::LDAP:
        {
            node->children.push_back(std::make_shared<ASTLiteral>(getLDAPServerName()));
            break;
        }
        case AuthenticationType::JWT:
        {
            throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "JWT is available only in ClickHouse Cloud");
        }
        case AuthenticationType::KERBEROS:
        {
            const auto & realm = getKerberosRealm();

            if (!realm.empty())
                node->children.push_back(std::make_shared<ASTLiteral>(realm));

            break;
        }
        case AuthenticationType::SSL_CERTIFICATE:
        {
#if USE_SSL
            using X509Certificate::Subjects::Type::CN;
            using X509Certificate::Subjects::Type::SAN;

            const auto &subjects = getSSLCertificateSubjects();
            X509Certificate::Subjects::Type cert_subject_type = !subjects.at(SAN).empty() ? SAN : CN;

            node->ssl_cert_subject_type = toString(cert_subject_type);
            for (const auto & name : getSSLCertificateSubjects().at(cert_subject_type))
                node->children.push_back(std::make_shared<ASTLiteral>(name));

            break;
#else
            throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "SSL certificates support is disabled, because ClickHouse was built without SSL library");
#endif
        }
        case AuthenticationType::SSH_KEY:
        {
#if USE_SSH
            for (const auto & key : getSSHKeys())
                node->children.push_back(std::make_shared<ASTPublicSSHKey>(key.getBase64(), key.getKeyType()));

            break;
#else
            throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "SSH is disabled, because ClickHouse is built without libssh");
#endif
        }
        case AuthenticationType::HTTP:
        {
            node->children.push_back(std::make_shared<ASTLiteral>(getHTTPAuthenticationServerName()));
            node->children.push_back(std::make_shared<ASTLiteral>(toString(getHTTPAuthenticationScheme())));
            break;
        }

        case AuthenticationType::NO_PASSWORD:
            break;
        case AuthenticationType::MAX:
            throw Exception(ErrorCodes::LOGICAL_ERROR, "AST: Unexpected authentication type {}", toString(auth_type));
    }


    if (valid_until)
    {
        WriteBufferFromOwnString out;
        writeDateTimeText(valid_until, out);

        node->valid_until = std::make_shared<ASTLiteral>(out.str());
    }

    return node;
}


AuthenticationData AuthenticationData::fromAST(const ASTAuthenticationData & query, ContextPtr context, bool validate)
{
    time_t valid_until = 0;

    if (query.valid_until)
    {
        valid_until = getValidUntilFromAST(query.valid_until, context);
    }

    if (query.type && query.type == AuthenticationType::NO_PASSWORD)
    {
        AuthenticationData auth_data;
        auth_data.setValidUntil(valid_until);
        return auth_data;
    }

    /// For this type of authentication we have ASTPublicSSHKey as children for ASTAuthenticationData
    if (query.type && query.type == AuthenticationType::SSH_KEY)
    {
#if USE_SSH
        AuthenticationData auth_data(*query.type);
        std::vector<SSHKey> keys;

        size_t args_size = query.children.size();
        for (size_t i = 0; i < args_size; ++i)
        {
            const auto & ssh_key = query.children[i]->as<ASTPublicSSHKey &>();
            const auto & key_base64 = ssh_key.key_base64;
            const auto & type = ssh_key.type;

            try
            {
                keys.emplace_back(SSHKeyFactory::makePublicKeyFromBase64(key_base64, type));
            }
            catch (const std::invalid_argument &)
            {
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Bad SSH key in entry: {} with type {}", key_base64, type);
            }
        }

        auth_data.setSSHKeys(std::move(keys));
        auth_data.setValidUntil(valid_until);
        return auth_data;
#else
        throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "SSH is disabled, because ClickHouse is built without libssh");
#endif
    }

    size_t args_size = query.children.size();
    ASTs args(args_size);
    for (size_t i = 0; i < args_size; ++i)
        args[i] = evaluateConstantExpressionAsLiteral(query.children[i], context);

    if (query.contains_password)
    {
        if (!query.type && !context)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot get default password type without context");

        if (validate && !context)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot check password complexity rules without context");

        if (query.type == AuthenticationType::BCRYPT_PASSWORD && !context)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot get bcrypt work factor without context");

        String value = checkAndGetLiteralArgument<String>(args[0], "password");

        AuthenticationType current_type;

        if (query.type)
            current_type = *query.type;
        else
            current_type = context->getAccessControl().getDefaultPasswordType();

        AuthenticationData auth_data(current_type);

        auth_data.setValidUntil(valid_until);

        if (validate)
            context->getAccessControl().checkPasswordComplexityRules(value);

        if (query.type == AuthenticationType::BCRYPT_PASSWORD)
        {
            int workfactor = context->getAccessControl().getBcryptWorkfactor();
            auth_data.setPasswordBcrypt(value, workfactor, validate);
            return auth_data;
        }

        if (query.type == AuthenticationType::SHA256_PASSWORD)
        {
#if USE_SSL
            /// random generator FIPS compliant
            uint8_t key[32];
            if (RAND_bytes(key, sizeof(key)) != 1)
                throw Exception(ErrorCodes::OPENSSL_ERROR, "RAND_bytes failed: {}", getOpenSSLErrors());

            String salt;
            salt.resize(sizeof(key) * 2);

            char * buf_pos = salt.data();
            for (uint8_t k : key)
            {
                writeHexByteUppercase(k, buf_pos);
                buf_pos += 2;
            }

            value.append(salt);
            auth_data.setSalt(salt);
#else
            throw Exception(ErrorCodes::SUPPORT_IS_DISABLED,
                            "SHA256 passwords support is disabled, because ClickHouse was built without SSL library");
#endif
        }

        if (query.type == AuthenticationType::SCRAM_SHA256_PASSWORD)
        {
#if USE_SSL
            /// random generator FIPS compliant
            uint8_t key[32];
            if (RAND_bytes(key, sizeof(key)) != 1)
                throw Exception(ErrorCodes::OPENSSL_ERROR, "RAND_bytes failed: {}", getOpenSSLErrors());

            String salt;
            salt.resize(sizeof(key) * 2);

            char * buf_pos = salt.data();
            for (uint8_t k : key)
            {
                writeHexByteUppercase(k, buf_pos);
                buf_pos += 2;
            }

            auth_data.setSalt(salt);
            auto digest = Util::encodeScramSHA256(value, salt);
            auth_data.setPasswordHashBinary(digest, validate);

            return auth_data;
#else
            throw Exception(ErrorCodes::SUPPORT_IS_DISABLED,
                            "SHA256 passwords support is disabled, because ClickHouse was built without SSL library");
#endif
        }


        auth_data.setPassword(value, validate);
        return auth_data;
    }

    AuthenticationData auth_data(*query.type);
    auth_data.setValidUntil(valid_until);

    if (query.contains_hash)
    {
        String value = checkAndGetLiteralArgument<String>(args[0], "hash");

        if (query.type == AuthenticationType::BCRYPT_PASSWORD)
        {
            auth_data.setPasswordHashBinary(AuthenticationData::Util::stringToDigest(value), validate);
            return auth_data;
        }

        auth_data.setPasswordHashHex(value, validate);

        if ((query.type == AuthenticationType::SHA256_PASSWORD || query.type == AuthenticationType::SCRAM_SHA256_PASSWORD)
            && args_size == 2)
        {
            String parsed_salt = checkAndGetLiteralArgument<String>(args[1], "salt");
            auth_data.setSalt(parsed_salt);
            return auth_data;
        }
    }
    else if (query.type == AuthenticationType::LDAP)
    {
        String value = checkAndGetLiteralArgument<String>(args[0], "ldap_server_name");
        auth_data.setLDAPServerName(value);
    }
    else if (query.type == AuthenticationType::KERBEROS)
    {
        if (!args.empty())
        {
            String value = checkAndGetLiteralArgument<String>(args[0], "kerberos_realm");
            auth_data.setKerberosRealm(value);
        }
    }
    else if (query.type == AuthenticationType::SSL_CERTIFICATE)
    {
#if USE_SSL
        auto ssl_cert_subject_type = X509Certificate::Subjects::parseSubjectType(*query.ssl_cert_subject_type);
        for (const auto & arg : args)
            auth_data.addSSLCertificateSubject(ssl_cert_subject_type, checkAndGetLiteralArgument<String>(arg, "ssl_certificate_subject"));
#else
        throw Exception(ErrorCodes::SUPPORT_IS_DISABLED, "SSL certificates support is disabled, because ClickHouse was built without SSL library");
#endif
    }
    else if (query.type == AuthenticationType::HTTP)
    {
        String server = checkAndGetLiteralArgument<String>(args[0], "http_auth_server_name");
        auto scheme = HTTPAuthenticationScheme::BASIC;  // Default scheme

        if (args_size > 1)
            scheme = parseHTTPAuthenticationScheme(checkAndGetLiteralArgument<String>(args[1], "scheme"));

        auth_data.setHTTPAuthenticationServerName(server);
        auth_data.setHTTPAuthenticationScheme(scheme);
    }
    else
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Unexpected ASTAuthenticationData structure");
    }

    return auth_data;
}

}
