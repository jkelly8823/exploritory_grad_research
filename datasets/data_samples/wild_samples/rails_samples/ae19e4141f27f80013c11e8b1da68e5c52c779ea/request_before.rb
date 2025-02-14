require 'tempfile'
require 'stringio'
require 'strscan'

require 'active_support/core_ext/hash/indifferent_access'
require 'active_support/core_ext/string/access'
require 'active_support/inflector'
require 'action_dispatch/http/headers'

module ActionDispatch
  class Request < Rack::Request
    include ActionDispatch::Http::Cache::Request
    include ActionDispatch::Http::MimeNegotiation
    include ActionDispatch::Http::Parameters
    include ActionDispatch::Http::FilterParameters
    include ActionDispatch::Http::Upload
    include ActionDispatch::Http::URL

    LOCALHOST = [/^127\.0\.0\.\d{1,3}$/, "::1", /^0:0:0:0:0:0:0:1(%.*)?$/].freeze

    %w[ AUTH_TYPE GATEWAY_INTERFACE
        PATH_TRANSLATED REMOTE_HOST
        REMOTE_IDENT REMOTE_USER REMOTE_ADDR
        SERVER_NAME SERVER_PROTOCOL

        HTTP_ACCEPT HTTP_ACCEPT_CHARSET HTTP_ACCEPT_ENCODING
        HTTP_ACCEPT_LANGUAGE HTTP_CACHE_CONTROL HTTP_FROM
        HTTP_NEGOTIATE HTTP_PRAGMA ].each do |env|
      class_eval <<-METHOD, __FILE__, __LINE__ + 1
        def #{env.sub(/^HTTP_/n, '').downcase}
          @env["#{env}"]
        end
      METHOD
    end

    def self.new(env)
      if request = env["action_dispatch.request"] && request.instance_of?(self)
        return request
      end

      super
    end

    def key?(key)
      @env.key?(key)
    end

    # List of HTTP request methods from the following RFCs:
    # Hypertext Transfer Protocol -- HTTP/1.1 (http://www.ietf.org/rfc/rfc2616.txt)
    # HTTP Extensions for Distributed Authoring -- WEBDAV (http://www.ietf.org/rfc/rfc2518.txt)
    # Versioning Extensions to WebDAV (http://www.ietf.org/rfc/rfc3253.txt)
    # Ordered Collections Protocol (WebDAV) (http://www.ietf.org/rfc/rfc3648.txt)
    # Web Distributed Authoring and Versioning (WebDAV) Access Control Protocol (http://www.ietf.org/rfc/rfc3744.txt)
    # Web Distributed Authoring and Versioning (WebDAV) SEARCH (http://www.ietf.org/rfc/rfc5323.txt)
    # PATCH Method for HTTP (http://www.ietf.org/rfc/rfc5789.txt)
    RFC2616 = %w(OPTIONS GET HEAD POST PUT DELETE TRACE CONNECT)
    RFC2518 = %w(PROPFIND PROPPATCH MKCOL COPY MOVE LOCK UNLOCK)
    RFC3253 = %w(VERSION-CONTROL REPORT CHECKOUT CHECKIN UNCHECKOUT MKWORKSPACE UPDATE LABEL MERGE BASELINE-CONTROL MKACTIVITY)
    RFC3648 = %w(ORDERPATCH)
    RFC3744 = %w(ACL)
    RFC5323 = %w(SEARCH)
    RFC5789 = %w(PATCH)

    HTTP_METHODS = RFC2616 + RFC2518 + RFC3253 + RFC3648 + RFC3744 + RFC5323 + RFC5789
    HTTP_METHOD_LOOKUP = Hash.new { |h, m| h[m] = m.underscore.to_sym if HTTP_METHODS.include?(m) }

    # Returns the HTTP \method that the application should see.
    # In the case where the \method was overridden by a middleware
    # (for instance, if a HEAD request was converted to a GET,
    # or if a _method parameter was used to determine the \method
    # the application should use), this \method returns the overridden
    # value, not the original.
    def request_method
      @request_method ||= check_method(env["REQUEST_METHOD"])
    end

    # Returns a symbol form of the #request_method
    def request_method_symbol
      HTTP_METHOD_LOOKUP[request_method]
    end

    # Returns the original value of the environment's REQUEST_METHOD,
    # even if it was overridden by middleware. See #request_method for
    # more information.
    def method
      @method ||= check_method(env["rack.methodoverride.original_method"] || env['REQUEST_METHOD'])
    end

    # Returns a symbol form of the #method
    def method_symbol
      HTTP_METHOD_LOOKUP[method]
    end

    # Is this a GET (or HEAD) request?
    # Equivalent to <tt>request.request_method == :get</tt>.
    def get?
      HTTP_METHOD_LOOKUP[request_method] == :get
    end

    # Is this a POST request?
    # Equivalent to <tt>request.request_method == :post</tt>.
    def post?
      HTTP_METHOD_LOOKUP[request_method] == :post
    end

    # Is this a PUT request?
    # Equivalent to <tt>request.request_method == :put</tt>.
    def put?
      HTTP_METHOD_LOOKUP[request_method] == :put
    end

    # Is this a DELETE request?
    # Equivalent to <tt>request.request_method == :delete</tt>.
    def delete?
      HTTP_METHOD_LOOKUP[request_method] == :delete
    end

    # Is this a HEAD request?
    # Equivalent to <tt>request.method == :head</tt>.
    def head?
      HTTP_METHOD_LOOKUP[method] == :head
    end

    # Provides access to the request's HTTP headers, for example:
    #
    #   request.headers["Content-Type"] # => "text/plain"
    def headers
      Http::Headers.new(@env)
    end

    def fullpath
      @fullpath ||= super
    end

    def forgery_whitelisted?
      get? || xhr? || content_mime_type.nil? || !content_mime_type.verify_request?
    end

    def media_type
      content_mime_type.to_s
    end

    # Returns the content length of the request as an integer.
    def content_length
      super.to_i
    end

    # Returns true if the "X-Requested-With" header contains "XMLHttpRequest"
    # (case-insensitive). All major JavaScript libraries send this header with
    # every Ajax request.
    def xml_http_request?
      @env['HTTP_X_REQUESTED_WITH'] =~ /XMLHttpRequest/i
    end
    alias :xhr? :xml_http_request?

    def ip
      @ip ||= super
    end

    # Which IP addresses are "trusted proxies" that can be stripped from
    # the right-hand-side of X-Forwarded-For.
    #
    # http://en.wikipedia.org/wiki/Private_network#Private_IPv4_address_spaces.
    TRUSTED_PROXIES = %r{
      ^127\.0\.0\.1$                | # localhost
      ^(10                          | # private IP 10.x.x.x
        172\.(1[6-9]|2[0-9]|3[0-1]) | # private IP in the range 172.16.0.0 .. 172.31.255.255
        192\.168                      # private IP 192.168.x.x
       )\.
    }x

    # Determines originating IP address.  REMOTE_ADDR is the standard
    # but will fail if the user is behind a proxy.  HTTP_CLIENT_IP and/or
    # HTTP_X_FORWARDED_FOR are set by proxies so check for these if
    # REMOTE_ADDR is a proxy.  HTTP_X_FORWARDED_FOR may be a comma-
    # delimited list in the case of multiple chained proxies; the last
    # address which is not trusted is the originating IP.
    def remote_ip
      @remote_ip ||= (@env["action_dispatch.remote_ip"] || ip).to_s
    end

    # Returns the lowercase name of the HTTP server software.
    def server_software
      (@env['SERVER_SOFTWARE'] && /^([a-zA-Z]+)/ =~ @env['SERVER_SOFTWARE']) ? $1.downcase : nil
    end

    # Read the request \body. This is useful for web services that need to
    # work with raw requests directly.
    def raw_post
      unless @env.include? 'RAW_POST_DATA'
        @env['RAW_POST_DATA'] = body.read(@env['CONTENT_LENGTH'].to_i)
        body.rewind if body.respond_to?(:rewind)
      end
      @env['RAW_POST_DATA']
    end

    # The request body is an IO input stream. If the RAW_POST_DATA environment
    # variable is already set, wrap it in a StringIO.
    def body
      if raw_post = @env['RAW_POST_DATA']
        raw_post.force_encoding(Encoding::BINARY) if raw_post.respond_to?(:force_encoding)
        StringIO.new(raw_post)
      else
        @env['rack.input']
      end
    end

    def form_data?
      FORM_DATA_MEDIA_TYPES.include?(content_mime_type.to_s)
    end

    def body_stream #:nodoc:
      @env['rack.input']
    end

    # TODO This should be broken apart into AD::Request::Session and probably
    # be included by the session middleware.
    def reset_session
      session.destroy if session && session.respond_to?(:destroy)
      self.session = {}