	zval_ptr_dtor(&pcount);
		
	while(count-- > 0) {
		spl_SplObjectStorageElement *pelement;
		char *hash;
		int hash_len;
		
		if (*p != ';') {
			goto outexcept;
		}
		++p;
		if(*p != 'O' && *p != 'C' && *p != 'r') {
			goto outexcept;
		}
		ALLOC_INIT_ZVAL(pentry);
		if (!php_var_unserialize(&pentry, &p, s + buf_len, &var_hash TSRMLS_CC)) {
			zval_ptr_dtor(&pentry);
			goto outexcept;
		}
		if(Z_TYPE_P(pentry) != IS_OBJECT) {
			zval_ptr_dtor(&pentry);
			goto outexcept;
		}
		ALLOC_INIT_ZVAL(pinf);
		if (*p == ',') { /* new version has inf */
			++p;
			if (!php_var_unserialize(&pinf, &p, s + buf_len, &var_hash TSRMLS_CC)) {
				goto outexcept;
			}
		}

		hash = spl_object_storage_get_hash(intern, getThis(), pentry, &hash_len TSRMLS_CC);
		if (!hash) {
			zval_ptr_dtor(&pentry);
			zval_ptr_dtor(&pinf);
			goto outexcept;
		}
		pelement = spl_object_storage_get(intern, hash, hash_len TSRMLS_CC);
		spl_object_storage_free_hash(intern, hash);
		if(pelement) {
			if(pelement->inf) {
				var_push_dtor(&var_hash, &pelement->inf);
			}
			if(pelement->obj) {
				var_push_dtor(&var_hash, &pelement->obj);
			}
		} 
		spl_object_storage_attach(intern, getThis(), pentry, pinf TSRMLS_CC);
		zval_ptr_dtor(&pentry);
		zval_ptr_dtor(&pinf);
	}