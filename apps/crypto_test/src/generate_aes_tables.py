#!/usr/bin/env python3

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
import os


AES_128_KEY = "aes_128_key"
AES_128_INPUT = "aes_128_input"
AES_128_ECB_EXPECTED = "aes_128_ecb_expected"
AES_128_CBC_IV = "aes_128_cbc_iv"
AES_128_CBC_EXPECTED = "aes_128_cbc_expected"
AES_128_CTR_NONCE = "aes_128_ctr_nonce"
AES_128_CTR_EXPECTED = "aes_128_ctr_expected"

LICENSE = """
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
"""


def write_header(f):
    f.write(LICENSE)
    f.write("\n/* Auto-generated file */\n\n")
    f.write("#include <stdint.h>\n\n")


def write_buffer(f, name, _type, buffer):
    f.write("{} {}[] = {{".format(_type, name))
    li = list(buffer)
    for i, el in enumerate(li):
        if i % 8 == 0:
            f.write("\n    ")
        else:
            f.write(" ")
        f.write("0x{:02x},".format(el))
    f.write("\n};\n\n")


def write_aes_128_ecb_tables(f, key, inbuf):
    backend = default_backend()
    cipher = Cipher(algorithms.AES(key), modes.ECB(), backend=backend)
    encryptor = cipher.encryptor()
    ct = encryptor.update(inbuf) + encryptor.finalize()
    write_buffer(f, AES_128_ECB_EXPECTED, "uint8_t", ct)


def write_aes_128_cbc_tables(f, key, inbuf, iv):
    backend = default_backend()
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv), backend=backend)
    encryptor = cipher.encryptor()
    ct = encryptor.update(inbuf) + encryptor.finalize()
    write_buffer(f, AES_128_CBC_EXPECTED, "uint8_t", ct)


def write_aes_128_ctr_tables(f, key, inbuf, nonce):
    backend = default_backend()
    cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=backend)
    encryptor = cipher.encryptor()
    ct = encryptor.update(inbuf) + encryptor.finalize()
    write_buffer(f, AES_128_CTR_EXPECTED, "uint8_t", ct)


if __name__ == "__main__":
    with open("tables.c", "w") as f:
        write_header(f)
        key = os.urandom(16)
        write_buffer(f, AES_128_KEY, "uint8_t", key)
        iv = os.urandom(16)
        write_buffer(f, AES_128_CBC_IV, "uint8_t", iv)
        nonce = os.urandom(16)
        write_buffer(f, AES_128_CTR_NONCE, "uint8_t", nonce)
        inbuf = os.urandom(4096)
        write_buffer(f, AES_128_INPUT, "uint8_t", inbuf)
        write_aes_128_ecb_tables(f, key, inbuf)
        write_aes_128_cbc_tables(f, key, inbuf, iv)
        write_aes_128_ctr_tables(f, key, inbuf, nonce)
