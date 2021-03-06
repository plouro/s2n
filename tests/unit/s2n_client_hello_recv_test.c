/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "s2n_test.h"

#include "testlib/s2n_testlib.h"

#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>

#include <s2n.h>

#include "tls/s2n_tls.h"
#include "tls/s2n_tls13.h"
#include "tls/s2n_connection.h"
#include "tls/s2n_client_hello.h"

#include "utils/s2n_safety.h"

int main(int argc, char **argv)
{
    struct s2n_connection *server_conn;
    struct s2n_connection *client_conn;
    struct s2n_stuffer *hello_stuffer;
    struct s2n_config *config;
    struct s2n_config *tls13_config;
    struct s2n_cert_chain_and_key *chain_and_key;
    struct s2n_cert_chain_and_key *tls13_chain_and_key;
    char *cert_chain;
    char *tls13_cert_chain;
    char *private_key;
    char *tls13_private_key;

    BEGIN_TEST();

    EXPECT_NOT_NULL(cert_chain = malloc(S2N_MAX_TEST_PEM_SIZE));
    EXPECT_NOT_NULL(private_key = malloc(S2N_MAX_TEST_PEM_SIZE));
    EXPECT_NOT_NULL(config = s2n_config_new());
    EXPECT_NOT_NULL(chain_and_key = s2n_cert_chain_and_key_new());
   
    EXPECT_NOT_NULL(tls13_cert_chain = malloc(S2N_MAX_TEST_PEM_SIZE));
    EXPECT_NOT_NULL(tls13_private_key = malloc(S2N_MAX_TEST_PEM_SIZE));
    EXPECT_NOT_NULL(tls13_config = s2n_config_new());
    EXPECT_NOT_NULL(tls13_chain_and_key = s2n_cert_chain_and_key_new());

    EXPECT_SUCCESS(s2n_read_test_pem(S2N_DEFAULT_TEST_CERT_CHAIN, cert_chain, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_read_test_pem(S2N_DEFAULT_TEST_PRIVATE_KEY, private_key, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_cert_chain_and_key_load_pem(chain_and_key, cert_chain, private_key));
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(config, chain_and_key));

    EXPECT_SUCCESS(s2n_read_test_pem(S2N_ECDSA_P384_PKCS1_CERT_CHAIN, tls13_cert_chain, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_read_test_pem(S2N_ECDSA_P384_PKCS1_KEY, tls13_private_key, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_cert_chain_and_key_load_pem(tls13_chain_and_key, tls13_cert_chain, tls13_private_key));
    EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(tls13_config, tls13_chain_and_key));

    /* These tests verify the logic behind the setting of these three connection fields: 
    server_protocol_version, client_protocol_version, and actual_protocol version. */ 
    
    /* Test that a tls12 client legacy version and tls12 server version 
    will successfully set a tls12 connection, since tls13 is not enabled. */
    {
        EXPECT_NOT_NULL(server_conn = s2n_connection_new(S2N_SERVER));
        EXPECT_NOT_NULL(client_conn = s2n_connection_new(S2N_CLIENT));
        EXPECT_SUCCESS(s2n_connection_set_config(server_conn, config));
        EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));
        
        EXPECT_SUCCESS(s2n_client_hello_send(client_conn));
        EXPECT_SUCCESS(s2n_stuffer_write(&server_conn->handshake.io, &client_conn->handshake.io.blob));
        EXPECT_SUCCESS(s2n_client_hello_recv(server_conn));

        EXPECT_SUCCESS(server_conn->server_protocol_version = S2N_TLS12);
        EXPECT_SUCCESS(server_conn->actual_protocol_version = S2N_TLS12);
        EXPECT_SUCCESS(server_conn->client_protocol_version = S2N_TLS12);

        s2n_connection_free(server_conn);
        s2n_connection_free(client_conn);
    }
    /* Test that a tls11 client legacy version and tls12 server version 
    will successfully set a tls11 connection. */
    {
        EXPECT_NOT_NULL(server_conn = s2n_connection_new(S2N_SERVER));
        EXPECT_NOT_NULL(client_conn = s2n_connection_new(S2N_CLIENT));
        EXPECT_SUCCESS(s2n_connection_set_config(server_conn, config));
        EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));

        client_conn->client_protocol_version = S2N_TLS11;
        
        EXPECT_SUCCESS(s2n_client_hello_send(client_conn));
        EXPECT_SUCCESS(s2n_stuffer_write(&server_conn->handshake.io, &client_conn->handshake.io.blob));
        EXPECT_SUCCESS(s2n_client_hello_recv(server_conn));

        EXPECT_SUCCESS(server_conn->server_protocol_version = S2N_TLS12);
        EXPECT_SUCCESS(server_conn->actual_protocol_version = S2N_TLS11);
        EXPECT_SUCCESS(server_conn->client_protocol_version = S2N_TLS11);

        s2n_connection_free(server_conn);
        s2n_connection_free(client_conn);
    }
    /* Test that a tls12 client and tls13 server will successfully 
    set a tls12 connection. */ 
    {
        EXPECT_NOT_NULL(client_conn = s2n_connection_new(S2N_CLIENT));
        EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));
        EXPECT_SUCCESS(s2n_enable_tls13());
        EXPECT_NOT_NULL(server_conn = s2n_connection_new(S2N_SERVER));
        EXPECT_SUCCESS(s2n_connection_set_config(server_conn, config));

        EXPECT_SUCCESS(s2n_client_hello_send(client_conn)); 
        
        EXPECT_SUCCESS(s2n_stuffer_write(&server_conn->handshake.io, &client_conn->handshake.io.blob));
        EXPECT_SUCCESS(s2n_client_hello_recv(server_conn)); 
      
        EXPECT_SUCCESS(server_conn->server_protocol_version = S2N_TLS13);
        EXPECT_SUCCESS(server_conn->actual_protocol_version = S2N_TLS12);
        EXPECT_SUCCESS(server_conn->client_protocol_version = S2N_TLS12);

        s2n_connection_free(server_conn);
        s2n_connection_free(client_conn);
        EXPECT_SUCCESS(s2n_disable_tls13());
    } 
    /* Test that an erroneous client legacy version and tls13 server version 
    will still successfully set a tls13 connection, when real client version is tls13. */
    {
        EXPECT_SUCCESS(s2n_enable_tls13());
        EXPECT_NOT_NULL(server_conn = s2n_connection_new(S2N_SERVER));
        EXPECT_NOT_NULL(client_conn = s2n_connection_new(S2N_CLIENT));
        EXPECT_SUCCESS(s2n_connection_set_config(server_conn, tls13_config));
        EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));

        EXPECT_SUCCESS(s2n_connection_set_cipher_preferences(client_conn, "default_tls13"));
        EXPECT_SUCCESS(s2n_connection_set_cipher_preferences(server_conn, "default_tls13"));

        hello_stuffer = &client_conn->handshake.io;

        EXPECT_SUCCESS(s2n_client_hello_send(client_conn)); 

        /* Overwrite the client legacy version so that it reads tls13 (incorrectly) */ 
        uint8_t incorrect_protocol_version[S2N_TLS_PROTOCOL_VERSION_LEN];
        incorrect_protocol_version[0] = S2N_TLS13 / 10;
        incorrect_protocol_version[1] = S2N_TLS13 % 10;
        EXPECT_SUCCESS(s2n_stuffer_rewrite(hello_stuffer));
        EXPECT_SUCCESS(s2n_stuffer_write_bytes(hello_stuffer, incorrect_protocol_version, S2N_TLS_PROTOCOL_VERSION_LEN));
        
        EXPECT_SUCCESS(s2n_stuffer_write(&server_conn->handshake.io, &hello_stuffer->blob));
        EXPECT_SUCCESS(s2n_client_hello_recv(server_conn)); 

        EXPECT_SUCCESS(server_conn->server_protocol_version = S2N_TLS13);
        EXPECT_SUCCESS(server_conn->actual_protocol_version = S2N_TLS13);
        EXPECT_SUCCESS(server_conn->client_protocol_version = S2N_TLS13);

        s2n_connection_free(server_conn);
        s2n_connection_free(client_conn);
        EXPECT_SUCCESS(s2n_disable_tls13());
    } 
    /* Test that a tls12 client legacy version and tls13 server version 
    will still successfully set a tls13 connection, if possible. */
    {
        EXPECT_SUCCESS(s2n_enable_tls13());
        EXPECT_NOT_NULL(server_conn = s2n_connection_new(S2N_SERVER));
        EXPECT_NOT_NULL(client_conn = s2n_connection_new(S2N_CLIENT));
        EXPECT_SUCCESS(s2n_connection_set_config(server_conn, tls13_config));
        EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));

        EXPECT_SUCCESS(s2n_connection_set_cipher_preferences(client_conn, "default_tls13"));
        EXPECT_SUCCESS(s2n_connection_set_cipher_preferences(server_conn, "default_tls13"));

        EXPECT_SUCCESS(s2n_client_hello_send(client_conn)); 
        
        EXPECT_SUCCESS(s2n_stuffer_write(&server_conn->handshake.io, &client_conn->handshake.io.blob));
        EXPECT_SUCCESS(s2n_client_hello_recv(server_conn)); 
       
        EXPECT_SUCCESS(server_conn->server_protocol_version = S2N_TLS13);
        EXPECT_SUCCESS(server_conn->actual_protocol_version = S2N_TLS13);
        EXPECT_SUCCESS(server_conn->client_protocol_version = S2N_TLS13);

        s2n_connection_free(server_conn);
        s2n_connection_free(client_conn);
        EXPECT_SUCCESS(s2n_disable_tls13());
    } 
     /* Test that an erroneous(tls13) client legacy version and tls13 server version 
    will still successfully set a tls12 connection, if tls12 is the true client version. */
    {
        EXPECT_NOT_NULL(client_conn = s2n_connection_new(S2N_CLIENT));
        EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));
        EXPECT_SUCCESS(s2n_enable_tls13());
        EXPECT_NOT_NULL(server_conn = s2n_connection_new(S2N_SERVER));
        EXPECT_SUCCESS(s2n_connection_set_config(server_conn, config));

        EXPECT_SUCCESS(s2n_connection_set_cipher_preferences(server_conn, "default_tls13"));
        
        hello_stuffer = &client_conn->handshake.io;

        EXPECT_SUCCESS(s2n_client_hello_send(client_conn)); 

        /* Overwrite the client legacy version so that it reads tls13 (incorrectly) */ 
        uint8_t incorrect_protocol_version[S2N_TLS_PROTOCOL_VERSION_LEN];
        incorrect_protocol_version[0] = S2N_TLS13 / 10;
        incorrect_protocol_version[1] = S2N_TLS13 % 10;
        EXPECT_SUCCESS(s2n_stuffer_rewrite(hello_stuffer));
        EXPECT_SUCCESS(s2n_stuffer_write_bytes(hello_stuffer, incorrect_protocol_version, S2N_TLS_PROTOCOL_VERSION_LEN));
        
        EXPECT_SUCCESS(s2n_stuffer_write(&server_conn->handshake.io, &hello_stuffer->blob));
        EXPECT_SUCCESS(s2n_client_hello_recv(server_conn)); 
    
        EXPECT_SUCCESS(server_conn->server_protocol_version = S2N_TLS13);
        EXPECT_SUCCESS(server_conn->actual_protocol_version = S2N_TLS12);
        EXPECT_SUCCESS(server_conn->client_protocol_version = S2N_TLS12);

        s2n_connection_free(server_conn);
        s2n_connection_free(client_conn);
        EXPECT_SUCCESS(s2n_disable_tls13());
    } 

    s2n_config_free(config);
    s2n_config_free(tls13_config);
    s2n_cert_chain_and_key_free(chain_and_key);
    free(cert_chain);
    free(private_key);
    s2n_cert_chain_and_key_free(tls13_chain_and_key);
    free(tls13_cert_chain);
    free(tls13_private_key);
    END_TEST();
    return 0;
}
