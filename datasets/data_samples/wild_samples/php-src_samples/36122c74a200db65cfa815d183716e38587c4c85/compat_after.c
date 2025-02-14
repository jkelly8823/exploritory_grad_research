	if (URI) {
			/* Use libxml functions otherwise its memory deallocation is screwed up */
			*qualified = xmlStrdup(URI);
			*qualified = xmlStrncat(*qualified, parser->_ns_separator, 1);
			*qualified = xmlStrncat(*qualified, name, xmlStrlen(name));
	} else {
		*qualified = xmlStrdup(name);
	}
{
	XML_Parser parser;

	parser = (XML_Parser) emalloc(sizeof(struct _XML_Parser));
	memset(parser, 0, sizeof(struct _XML_Parser));
	parser->use_namespace = 0;
	parser->_ns_separator = NULL;

	parser->parser = xmlCreatePushParserCtxt((xmlSAXHandlerPtr) &php_xml_compat_handlers, (void *) parser, NULL, 0, NULL);
	if (parser->parser == NULL) {
		efree(parser);
		return NULL;
	}
	if (sep != NULL) {
		parser->use_namespace = 1;
		parser->parser->sax2 = 1;
		parser->_ns_separator = xmlStrdup(sep);
	} else {
		/* Reset flag as XML_SAX2_MAGIC is needed for xmlCreatePushParserCtxt 
		so must be set in the handlers */
		parser->parser->sax->initialized = 1;
	}
{
	if (parser->use_namespace) {
		if (parser->_ns_separator) {
			xmlFree(parser->_ns_separator);
		}
	}
	if (parser->parser->myDoc) {
		xmlFreeDoc(parser->parser->myDoc);
		parser->parser->myDoc = NULL;
	}
	xmlFreeParserCtxt(parser->parser);
	efree(parser);
}