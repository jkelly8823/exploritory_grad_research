{
	BUG_ON(index_key->type == NULL);
	kenter("%d,%s,", keyring->serial, index_key->type->name);

	if (index_key->type == &key_type_keyring)
		up_write(&keyring_serialise_link_sem);

	if (edit && !edit->dead_leaf) {
		key_payload_reserve(keyring,
				    keyring->datalen - KEYQUOTA_LINK_BYTES);
		assoc_array_cancel_edit(edit);
	}