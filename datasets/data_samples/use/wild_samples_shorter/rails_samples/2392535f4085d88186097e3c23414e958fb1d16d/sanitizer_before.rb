
    # A regular expression of the valid characters used to separate protocols like
    # the ':' in 'http://foo.com'
    self.protocol_separator     = /:|(&#0*58)|(&#x70)|(%|&#37;)3A/

    # Specifies a Set of HTML attributes that can have URIs.
    self.uri_attributes         = Set.new(%w(href src cite action longdesc xlink:href lowsrc))

      style = style.to_s.gsub(/url\s*\(\s*[^\s)]+?\s*\)\s*/, ' ')

      # gauntlet
      if style !~ /^([:,;#%.\sa-zA-Z0-9!]|\w-\w|\'[\s\w]+\'|\"[\s\w]+\"|\([\d,\s]+\))*$/ ||
          style !~ /^(\s*[-\w]+\s*:\s*[^:;]*(;|$)\s*)*$/
        return ''
      end

      clean = []
        elsif shorthand_css_properties.include?(prop.split('-')[0].downcase)
          unless val.split().any? do |keyword|
            !allowed_css_keywords.include?(keyword) &&
              keyword !~ /^(#[0-9a-f]+|rgb\(\d+%?,\d*%?,?\d*%?\)?|\d{0,2}\.?\d{0,2}(cm|em|ex|in|mm|pc|pt|px|%|,|\))?)$/
          end
            clean << prop + ': ' + val + ';'
          end
        end

    def contains_bad_protocols?(attr_name, value)
      uri_attributes.include?(attr_name) &&
      (value =~ /(^[^\/:]*):|(&#0*58)|(&#x70)|(%|&#37;)3A/ && !allowed_protocols.include?(value.split(protocol_separator).first.downcase.strip))
    end
  end
end