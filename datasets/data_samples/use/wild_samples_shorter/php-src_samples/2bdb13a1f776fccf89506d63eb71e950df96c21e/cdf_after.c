				goto out;
			}
			nelements = CDF_GETUINT32(q, 1);
			if (nelements > CDF_ELEMENT_LIMIT || nelements == 0) {
				DPRINTF(("CDF_VECTOR with nelements == %"
				    SIZE_T_FORMAT "u\n", nelements));
				goto out;
			}
			slen = 2;
		} else {
					goto out;
				inp += nelem;
			}
			for (j = 0; j < nelements && i < sh.sh_properties;
			    j++, i++)
			{
				uint32_t l;