idf_component_register(SRCS "src/SSLClient.cpp" "src/ssl_client.cpp"
                    INCLUDE_DIRS "src"
                    PRIV_REQUIRES "mbedtls" "arduino"
                    )
