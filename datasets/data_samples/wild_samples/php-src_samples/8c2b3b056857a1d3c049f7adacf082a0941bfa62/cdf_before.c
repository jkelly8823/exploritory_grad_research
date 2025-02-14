			if (left < sizeof(uint32_t) * 2) {
				DPRINTF(("missing CDF_VECTOR length\n"));
				goto out;
			}
			nelements = CDF_GETUINT32(q, 1);
			if (nelements == 0) {
				DPRINTF(("CDF_VECTOR with nelements == 0\n"));
				goto out;
			}
			slen = 2;
		} else {
			nelements = 1;
			slen = 1;
		}
		o4 = slen * sizeof(uint32_t);
		if (inp[i].pi_type & (CDF_ARRAY|CDF_BYREF|CDF_RESERVED))
			goto unknown;
		switch (inp[i].pi_type & CDF_TYPEMASK) {
		case CDF_NULL:
		case CDF_EMPTY:
			break;
		case CDF_SIGNED16:
			if (!cdf_copy_info(&inp[i], &q[o4], e, sizeof(int16_t)))
				goto unknown;
			break;
		case CDF_SIGNED32:
		case CDF_BOOL:
		case CDF_UNSIGNED32:
		case CDF_FLOAT:
			if (!cdf_copy_info(&inp[i], &q[o4], e, sizeof(int32_t)))
				goto unknown;
			break;
		case CDF_SIGNED64:
		case CDF_UNSIGNED64:
		case CDF_DOUBLE:
		case CDF_FILETIME:
			if (!cdf_copy_info(&inp[i], &q[o4], e, sizeof(int64_t)))
				goto unknown;
			break;
		case CDF_LENGTH32_STRING:
		case CDF_LENGTH32_WSTRING:
			if (nelements > 1) {
				size_t nelem = inp - *info;
				inp = cdf_grow_info(info, maxcount, nelements);
				if (inp == NULL)
					goto out;
				inp += nelem;
			}
			if (nelements > 1) {
				size_t nelem = inp - *info;
				inp = cdf_grow_info(info, maxcount, nelements);
				if (inp == NULL)
					goto out;
				inp += nelem;
			}
			DPRINTF(("nelements = %" SIZE_T_FORMAT "u\n",
			    nelements));
			for (j = 0; j < nelements && i < sh.sh_properties;
			    j++, i++)
			{
				uint32_t l;

				if (o4 + sizeof(uint32_t) > left)
					goto out;

				l = CDF_GETUINT32(q, slen);
				o4 += sizeof(uint32_t);
				if (o4 + l > left)
					goto out;

				inp[i].pi_str.s_len = l;
				inp[i].pi_str.s_buf = CAST(const char *,
				    CAST(const void *, &q[o4]));

				DPRINTF(("o=%zu l=%d(%" SIZE_T_FORMAT
				    "u), t=%zu s=%s\n", o4, l,
				    CDF_ROUND(l, sizeof(l)), left,
				    inp[i].pi_str.s_buf));

				if (l & 1)
					l++;

				slen += l >> 1;
				o4 = slen * sizeof(uint32_t);
			}
			i--;
			break;
		case CDF_CLIPBOARD:
			if (inp[i].pi_type & CDF_VECTOR)
				goto unknown;
			break;
		default:
		unknown:
			memset(&inp[i].pi_val, 0, sizeof(inp[i].pi_val));
			DPRINTF(("Don't know how to deal with %#x\n",
			    inp[i].pi_type));
			break;
		}
	}
	return 0;
out:
	efree(*info);
	*info = NULL;
	*count = 0;
	*maxcount = 0;
	errno = EFTYPE;
	return -1;
}

int
cdf_unpack_summary_info(const cdf_stream_t *sst, const cdf_header_t *h,
    cdf_summary_info_header_t *ssi, cdf_property_info_t **info, size_t *count)
{
	size_t maxcount;
	const cdf_summary_info_header_t *si =
	    CAST(const cdf_summary_info_header_t *, sst->sst_tab);
	const cdf_section_declaration_t *sd =
	    CAST(const cdf_section_declaration_t *, (const void *)
	    ((const char *)sst->sst_tab + CDF_SECTION_DECLARATION_OFFSET));

	if (cdf_check_stream_offset(sst, h, si, sizeof(*si), __LINE__) == -1 ||
	    cdf_check_stream_offset(sst, h, sd, sizeof(*sd), __LINE__) == -1)
		return -1;
	ssi->si_byte_order = CDF_TOLE2(si->si_byte_order);
	ssi->si_os_version = CDF_TOLE2(si->si_os_version);
	ssi->si_os = CDF_TOLE2(si->si_os);
	ssi->si_class = si->si_class;
	cdf_swap_class(&ssi->si_class);
	ssi->si_count = CDF_TOLE4(si->si_count);
	*count = 0;
	maxcount = 0;
	*info = NULL;
	if (cdf_read_property_info(sst, h, CDF_TOLE4(sd->sd_offset), info,
	    count, &maxcount) == -1)
		return -1;
	return 0;
}


#define extract_catalog_field(t, f, l) \
    if (b + l + sizeof(cep->f) > eb) { \
	    cep->ce_namlen = 0; \
	    break; \
    } \
    memcpy(&cep->f, b + (l), sizeof(cep->f)); \
    ce[i].f = CAST(t, CDF_TOLE(cep->f))

int
cdf_unpack_catalog(const cdf_header_t *h, const cdf_stream_t *sst,
    cdf_catalog_t **cat)
{
	size_t ss = cdf_check_stream(sst, h);
	const char *b = CAST(const char *, sst->sst_tab);
	const char *nb, *eb = b + ss * sst->sst_len;
	size_t nr, i, j, k;
	cdf_catalog_entry_t *ce;
	uint16_t reclen;
	const uint16_t *np;

	for (nr = 0;; nr++) {
		memcpy(&reclen, b, sizeof(reclen));
		reclen = CDF_TOLE2(reclen);
		if (reclen == 0)
			break;
		b += reclen;
		if (b > eb)
		    break;
	}
	if (nr == 0)
		return -1;
	nr--;
	*cat = CAST(cdf_catalog_t *,
	    CDF_MALLOC(sizeof(cdf_catalog_t) + nr * sizeof(*ce)));
	if (*cat == NULL)
		return -1;
	ce = (*cat)->cat_e;
	memset(ce, 0, nr * sizeof(*ce));
	b = CAST(const char *, sst->sst_tab);
	for (j = i = 0; i < nr; b += reclen) {
		cdf_catalog_entry_t *cep = &ce[j];
		uint16_t rlen;

		extract_catalog_field(uint16_t, ce_namlen, 0);
		extract_catalog_field(uint16_t, ce_num, 4);
		extract_catalog_field(uint64_t, ce_timestamp, 8);
		reclen = cep->ce_namlen;

		if (reclen < 14) {
			cep->ce_namlen = 0;
			continue;
		}

		cep->ce_namlen = __arraycount(cep->ce_name) - 1;
		rlen = reclen - 14;
		if (cep->ce_namlen > rlen)
			cep->ce_namlen = rlen;

		np = CAST(const uint16_t *, CAST(const void *, (b + 16)));
		nb = CAST(const char *, CAST(const void *,
		    (np + cep->ce_namlen)));
		if (nb > eb) {
			cep->ce_namlen = 0;
			break;
		}

		for (k = 0; k < cep->ce_namlen; k++)
			cep->ce_name[k] = np[k]; /* XXX: CDF_TOLE2? */
		cep->ce_name[cep->ce_namlen] = 0;
		j = i;
		i++;
	}
	(*cat)->cat_num = j;
	return 0;
}

int
cdf_print_classid(char *buf, size_t buflen, const cdf_classid_t *id)
{
	return snprintf(buf, buflen, "%.8x-%.4x-%.4x-%.2x%.2x-"
	    "%.2x%.2x%.2x%.2x%.2x%.2x", id->cl_dword, id->cl_word[0],
	    id->cl_word[1], id->cl_two[0], id->cl_two[1], id->cl_six[0],
	    id->cl_six[1], id->cl_six[2], id->cl_six[3], id->cl_six[4],
	    id->cl_six[5]);
}

static const struct {
	uint32_t v;
	const char *n;
} vn[] = {
	{ CDF_PROPERTY_CODE_PAGE, "Code page" },
	{ CDF_PROPERTY_TITLE, "Title" },
	{ CDF_PROPERTY_SUBJECT, "Subject" },
	{ CDF_PROPERTY_AUTHOR, "Author" },
	{ CDF_PROPERTY_KEYWORDS, "Keywords" },
	{ CDF_PROPERTY_COMMENTS, "Comments" },
	{ CDF_PROPERTY_TEMPLATE, "Template" },
	{ CDF_PROPERTY_LAST_SAVED_BY, "Last Saved By" },
	{ CDF_PROPERTY_REVISION_NUMBER, "Revision Number" },
	{ CDF_PROPERTY_TOTAL_EDITING_TIME, "Total Editing Time" },
	{ CDF_PROPERTY_LAST_PRINTED, "Last Printed" },
	{ CDF_PROPERTY_CREATE_TIME, "Create Time/Date" },
	{ CDF_PROPERTY_LAST_SAVED_TIME, "Last Saved Time/Date" },
	{ CDF_PROPERTY_NUMBER_OF_PAGES, "Number of Pages" },
	{ CDF_PROPERTY_NUMBER_OF_WORDS, "Number of Words" },
	{ CDF_PROPERTY_NUMBER_OF_CHARACTERS, "Number of Characters" },
	{ CDF_PROPERTY_THUMBNAIL, "Thumbnail" },
	{ CDF_PROPERTY_NAME_OF_APPLICATION, "Name of Creating Application" },
	{ CDF_PROPERTY_SECURITY, "Security" },
	{ CDF_PROPERTY_LOCALE_ID, "Locale ID" },
};

int
cdf_print_property_name(char *buf, size_t bufsiz, uint32_t p)
{
	size_t i;

	for (i = 0; i < __arraycount(vn); i++)
		if (vn[i].v == p)
			return snprintf(buf, bufsiz, "%s", vn[i].n);
	return snprintf(buf, bufsiz, "%#x", p);
}

int
cdf_print_elapsed_time(char *buf, size_t bufsiz, cdf_timestamp_t ts)
{
	int len = 0;
	int days, hours, mins, secs;

	ts /= CDF_TIME_PREC;
	secs = (int)(ts % 60);
	ts /= 60;
	mins = (int)(ts % 60);
	ts /= 60;
	hours = (int)(ts % 24);
	ts /= 24;
	days = (int)ts;

	if (days) {
		len += snprintf(buf + len, bufsiz - len, "%dd+", days);
		if ((size_t)len >= bufsiz)
			return len;
	}

	if (days || hours) {
		len += snprintf(buf + len, bufsiz - len, "%.2d:", hours);
		if ((size_t)len >= bufsiz)
			return len;
	}

	len += snprintf(buf + len, bufsiz - len, "%.2d:", mins);
	if ((size_t)len >= bufsiz)
		return len;

	len += snprintf(buf + len, bufsiz - len, "%.2d", secs);
	return len;
}

char *
cdf_u16tos8(char *buf, size_t len, const uint16_t *p)
{
	size_t i;
	for (i = 0; i < len && p[i]; i++)
		buf[i] = (char)p[i];
	buf[i] = '\0';
	return buf;
}

#ifdef CDF_DEBUG
void
cdf_dump_header(const cdf_header_t *h)
{
	size_t i;

#define DUMP(a, b) (void)fprintf(stderr, "%40.40s = " a "\n", # b, h->h_ ## b)
#define DUMP2(a, b) (void)fprintf(stderr, "%40.40s = " a " (" a ")\n", # b, \
    h->h_ ## b, 1 << h->h_ ## b)
	DUMP("%d", revision);
	DUMP("%d", version);
	DUMP("%#x", byte_order);
	DUMP2("%d", sec_size_p2);
	DUMP2("%d", short_sec_size_p2);
	DUMP("%d", num_sectors_in_sat);
	DUMP("%d", secid_first_directory);
	DUMP("%d", min_size_standard_stream);
	DUMP("%d", secid_first_sector_in_short_sat);
	DUMP("%d", num_sectors_in_short_sat);
	DUMP("%d", secid_first_sector_in_master_sat);
	DUMP("%d", num_sectors_in_master_sat);
	for (i = 0; i < __arraycount(h->h_master_sat); i++) {
		if (h->h_master_sat[i] == CDF_SECID_FREE)
			break;
		(void)fprintf(stderr, "%35.35s[%.3" SIZE_T_FORMAT "u] = %d\n",
		    "master_sat", i, h->h_master_sat[i]);
	}
}

void
cdf_dump_sat(const char *prefix, const cdf_sat_t *sat, size_t size)
{
	size_t i, j, s = size / sizeof(cdf_secid_t);

	for (i = 0; i < sat->sat_len; i++) {
		(void)fprintf(stderr, "%s[%" SIZE_T_FORMAT "u]:\n%.6"
		    SIZE_T_FORMAT "u: ", prefix, i, i * s);
		for (j = 0; j < s; j++) {
			(void)fprintf(stderr, "%5d, ",
			    CDF_TOLE4(sat->sat_tab[s * i + j]));
			if ((j + 1) % 10 == 0)
				(void)fprintf(stderr, "\n%.6" SIZE_T_FORMAT
				    "u: ", i * s + j + 1);
		}