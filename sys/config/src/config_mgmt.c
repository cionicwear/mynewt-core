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

#include "os/mynewt.h"

#if MYNEWT_VAL(CONFIG_MGMT)

#include <string.h>

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "config/config.h"
#include "config_priv.h"

#define CONFIG_MGMT_READ    (MYNEWT_VAL(CONFIG_MGMT_RW) & 1)
#define CONFIG_MGMT_WRITE   (MYNEWT_VAL(CONFIG_MGMT_RW) & 2)

#if CONFIG_MGMT_READ
static int conf_mgmt_read(struct mgmt_ctxt *);
#endif
#if CONFIG_MGMT_WRITE
static int conf_mgmt_write(struct mgmt_ctxt *);
#endif

static const struct mgmt_handler conf_mgmt_handlers[] = {
    [CONF_NMGR_OP] = {
#if CONFIG_MGMT_READ
        conf_mgmt_read,
#else
        NULL,
#endif
#if CONFIG_MGMT_WRITE
        conf_mgmt_write,
#else
        NULL,
#endif
    }
};

static struct mgmt_group conf_mgmt_group = {
    .mg_handlers = (struct mgmt_handler *)conf_mgmt_handlers,
    .mg_handlers_count = 1,
    .mg_group_id = MGMT_GROUP_ID_CONFIG
};

#if CONFIG_MGMT_READ
static int
conf_mgmt_read(struct mgmt_ctxt *cb)
{
    int rc;
    char name_str[CONF_MAX_NAME_LEN];
    char val_str[CONF_MAX_VAL_LEN];
    char *val;
    CborError g_err = CborNoError;

    const struct cbor_attr_t attr[2] = {
       #if !MYNEWT_VAL(UNITTEST)
        [0] = {
            .attribute = "name",
            .type = CborAttrTextStringType,
            .addr.string = name_str,
            .len = sizeof(name_str)
        },
        [1] = {
            .attribute = NULL
        }
        #endif
    };

    rc = cbor_read_object(&cb->it, attr);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    val = conf_get_value(name_str, val_str, sizeof(val_str));
    if (!val) {
        return MGMT_ERR_EINVAL;
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "val");
    g_err |= cbor_encode_text_stringz(&cb->encoder, val);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;
}
#endif

#if CONFIG_MGMT_WRITE
static int
conf_mgmt_write(struct mgmt_ctxt *cb)
{
    int rc;
    char name_str[CONF_MAX_NAME_LEN];
    char val_str[CONF_MAX_VAL_LEN];
    bool do_save = false;
    const struct cbor_attr_t val_attr[] = {
        #if !MYNEWT_VAL(UNITTEST)
        [0] = {
            .attribute = "name",
            .type = CborAttrTextStringType,
            .addr.string = name_str,
            .len = sizeof(name_str)
        },
        [1] = {
            .attribute = "val",
            .type = CborAttrTextStringType,
            .addr.string = val_str,
            .len = sizeof(val_str)
        },
        [2] = {
            .attribute = "save",
            .type = CborAttrBooleanType,
            .addr.boolean = &do_save
        },
        [3] = {
            .attribute = NULL
        }
        #endif
    };

    name_str[0] = '\0';
    val_str[0] = '\0';

    rc = cbor_read_object(&cb->it, val_attr);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    if (name_str[0] != '\0') {
        if (val_str[0] != '\0') {
            rc = conf_set_value(name_str, val_str);
        } else {
            rc = conf_set_value(name_str, NULL);
        }
        if (rc) {
            return MGMT_ERR_EINVAL;
        }
    }
    rc = conf_commit(NULL);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }
    if (do_save) {
        rc = conf_save();
        if (rc) {
            return MGMT_ERR_EINVAL;
        }
    }
    return 0;
}
#endif

int
conf_mgmt_register(void)
{
     mgmt_register_group(&conf_mgmt_group);
     return 0;
}
#endif
