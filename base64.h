/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE64_H
#define BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

/* Simple BASE64 encode/decode functions.
 *
 * As we might encode binary strings, hence we require the length of
 * the incoming plain source. And return the length of what we decoded.
 *
 * The decoding function takes any non valid char (i.e. whitespace, \0
 * or anything non A-Z,0-9 etc as terminal.
 *
 * plain strings/binary sequences are not assumed '\0' terminated. Encoded
 * strings are neither. But probably should.
 *
 */

/**
 * Given the length of an un-encoded string, get the length of the
 * encoded string.
 * @param len the length of an unencoded string.
 * @return the length of the string after it is encoded, including the
 * trailing \0
 */
#define base64_encode_len(len) ((((int)(len) + 2) / 3 * 4) + 1)

/**
 * Encode a text string using base64encoding.
 * @param coded_dst The destination string for the encoded string.
 * @param plain_src The original string in plain text
 * @param len_plain_src The length of the plain text string
 * @return the length of the encoded string
 */
int base64_encode(char *coded_dst, const unsigned char *plain_src,
    int len_plain_src);

#ifdef __cplusplus
}
#endif

#endif
