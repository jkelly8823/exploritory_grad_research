# frozen_string_literal: true

require "active_support/core_ext/object/try"
require "active_support/core_ext/kernel/singleton_class"
require "active_support/deprecation"
require "thread"
require "delegate"

module ActionView
  # = Action View Template
  class Template
    extend ActiveSupport::Autoload

    def self.finalize_compiled_template_methods
      ActiveSupport::Deprecation.warn "ActionView::Template.finalize_compiled_template_methods is deprecated and has no effect"
    end

    def self.finalize_compiled_template_methods=(_)
      ActiveSupport::Deprecation.warn "ActionView::Template.finalize_compiled_template_methods= is deprecated and has no effect"
    end

    # === Encodings in ActionView::Template
    #
    # ActionView::Template is one of a few sources of potential
    # encoding issues in Rails. This is because the source for
    # templates are usually read from disk, and Ruby (like most
    # encoding-aware programming languages) assumes that the
    # String retrieved through File IO is encoded in the
    # <tt>default_external</tt> encoding. In Rails, the default
    # <tt>default_external</tt> encoding is UTF-8.
    #
    # As a result, if a user saves their template as ISO-8859-1
    # (for instance, using a non-Unicode-aware text editor),
    # and uses characters outside of the ASCII range, their
    # users will see diamonds with question marks in them in
    # the browser.
    #
    # For the rest of this documentation, when we say "UTF-8",
    # we mean "UTF-8 or whatever the default_internal encoding
    # is set to". By default, it will be UTF-8.
    #
    # To mitigate this problem, we use a few strategies:
    # 1. If the source is not valid UTF-8, we raise an exception
    #    when the template is compiled to alert the user
    #    to the problem.
    # 2. The user can specify the encoding using Ruby-style
    #    encoding comments in any template engine. If such
    #    a comment is supplied, Rails will apply that encoding
    #    to the resulting compiled source returned by the
    #    template handler.
    # 3. In all cases, we transcode the resulting String to
    #    the UTF-8.
    #
    # This means that other parts of Rails can always assume
    # that templates are encoded in UTF-8, even if the original
    # source of the template was not UTF-8.
    #
    # From a user's perspective, the easiest thing to do is
    # to save your templates as UTF-8. If you do this, you
    # do not need to do anything else for things to "just work".
    #
    # === Instructions for template handlers
    #
    # The easiest thing for you to do is to simply ignore
    # encodings. Rails will hand you the template source
    # as the default_internal (generally UTF-8), raising
    # an exception for the user before sending the template
    # to you if it could not determine the original encoding.
    #
    # For the greatest simplicity, you can support only
    # UTF-8 as the <tt>default_internal</tt>. This means
    # that from the perspective of your handler, the
    # entire pipeline is just UTF-8.
    #
    # === Advanced: Handlers with alternate metadata sources
    #
    # If you want to provide an alternate mechanism for
    # specifying encodings (like ERB does via <%# encoding: ... %>),
    # you may indicate that you will handle encodings yourself
    # by implementing <tt>handles_encoding?</tt> on your handler.
    #
    # If you do, Rails will not try to encode the String
    # into the default_internal, passing you the unaltered
    # bytes tagged with the assumed encoding (from
    # default_external).
    #
    # In this case, make sure you return a String from
    # your handler encoded in the default_internal. Since
    # you are handling out-of-band metadata, you are
    # also responsible for alerting the user to any
    # problems with converting the user's data to
    # the <tt>default_internal</tt>.
    #
    # To do so, simply raise +WrongEncodingError+ as follows:
    #
    #     raise WrongEncodingError.new(
    #       problematic_string,
    #       expected_encoding
    #     )

    ##
    # :method: local_assigns
    #
    # Returns a hash with the defined local variables.
    #
    # Given this sub template rendering:
    #
    #   <%= render "shared/header", { headline: "Welcome", person: person } %>
    #
    # You can use +local_assigns+ in the sub templates to access the local variables:
    #
    #   local_assigns[:headline] # => "Welcome"

    eager_autoload do
      autoload :Error
      autoload :File
      autoload :Handlers
      autoload :HTML
      autoload :Inline
      autoload :Text
      autoload :Types
    end

    extend Template::Handlers

    attr_reader :source, :identifier, :handler, :original_encoding, :updated_at
    attr_reader :variable, :format, :variant, :locals, :virtual_path

    def initialize(source, identifier, handler, format: nil, variant: nil, locals: nil, virtual_path: nil, updated_at: nil)
      unless locals
        ActiveSupport::Deprecation.warn "ActionView::Template#initialize requires a locals parameter"
        locals = []
      end

      @source            = source
      @identifier        = identifier
      @handler           = handler
      @compiled          = false
      @locals            = locals
      @virtual_path      = virtual_path

      @variable = if @virtual_path
        base = @virtual_path[-1] == "/" ? "" : ::File.basename(@virtual_path)
        base =~ /\A_?(.*?)(?:\.\w+)*\z/
        $1.to_sym
      end

      if updated_at
        ActiveSupport::Deprecation.warn "ActionView::Template#updated_at is deprecated"
        @updated_at        = updated_at
      else
        @updated_at        = Time.now
      end
      @format            = format
      @variant           = variant
      @compile_mutex     = Mutex.new
    end

    deprecate :original_encoding
    deprecate :updated_at
    deprecate def virtual_path=(_); end
    deprecate def locals=(_); end
    deprecate def formats=(_); end
    deprecate def formats; Array(format); end
    deprecate def variants=(_); end
    deprecate def variants; [variant]; end

    # Returns whether the underlying handler supports streaming. If so,
    # a streaming buffer *may* be passed when it starts rendering.
    def supports_streaming?
      handler.respond_to?(:supports_streaming?) && handler.supports_streaming?
    end

    # Render a template. If the template was not compiled yet, it is done
    # exactly before rendering.
    #
    # This method is instrumented as "!render_template.action_view". Notice that
    # we use a bang in this instrumentation because you don't want to
    # consume this in production. This is only slow if it's being listened to.
    def render(view, locals, buffer = ActionView::OutputBuffer.new, &block)
      instrument_render_template do
        compile!(view)
        view._run(method_name, self, locals, buffer, &block)
      end
    rescue => e
      handle_render_error(view, e)
    end

    def type
      @type ||= Types[format]
    end

    # Receives a view object and return a template similar to self by using @virtual_path.
    #
    # This method is useful if you have a template object but it does not contain its source
    # anymore since it was already compiled. In such cases, all you need to do is to call
    # refresh passing in the view object.
    #
    # Notice this method raises an error if the template to be refreshed does not have a
    # virtual path set (true just for inline templates).
    def refresh(view)
      raise "A template needs to have a virtual path in order to be refreshed" unless @virtual_path
      lookup  = view.lookup_context
      pieces  = @virtual_path.split("/")
      name    = pieces.pop
      partial = !!name.sub!(/^_/, "")
      lookup.disable_cache do
        lookup.find_template(name, [ pieces.join("/") ], partial, @locals)
      end
    end

    def short_identifier
      @short_identifier ||= defined?(Rails.root) ? identifier.sub("#{Rails.root}/", "") : identifier
    end

    def inspect
      "#<#{self.class.name} #{short_identifier} locals=#{@locals.inspect}>"
    end

    # This method is responsible for properly setting the encoding of the
    # source. Until this point, we assume that the source is BINARY data.
    # If no additional information is supplied, we assume the encoding is
    # the same as <tt>Encoding.default_external</tt>.
    #
    # The user can also specify the encoding via a comment on the first
    # line of the template (# encoding: NAME-OF-ENCODING). This will work
    # with any template engine, as we process out the encoding comment
    # before passing the source on to the template engine, leaving a
    # blank line in its stead.
    def encode!
      source = self.source

      return source unless source.encoding == Encoding::BINARY

      # Look for # encoding: *. If we find one, we'll encode the
      # String in that encoding, otherwise, we'll use the
      # default external encoding.
      if source.sub!(/\A#{ENCODING_FLAG}/, "")
        encoding = magic_encoding = $1
      else
        encoding = Encoding.default_external
      end

      # Tag the source with the default external encoding
      # or the encoding specified in the file
      source.force_encoding(encoding)

      # If the user didn't specify an encoding, and the handler
      # handles encodings, we simply pass the String as is to
      # the handler (with the default_external tag)
      if !magic_encoding && @handler.respond_to?(:handles_encoding?) && @handler.handles_encoding?
        source
      # Otherwise, if the String is valid in the encoding,
      # encode immediately to default_internal. This means
      # that if a handler doesn't handle encodings, it will
      # always get Strings in the default_internal
      elsif source.valid_encoding?
        source.encode!
      # Otherwise, since the String is invalid in the encoding
      # specified, raise an exception
      else
        raise WrongEncodingError.new(source, encoding)
      end
    end


    # Exceptions are marshalled when using the parallel test runner with DRb, so we need
    # to ensure that references to the template object can be marshalled as well. This means forgoing
    # the marshalling of the compiler mutex and instantiating that again on unmarshalling.
    def marshal_dump # :nodoc:
      [ @source, @identifier, @handler, @compiled, @locals, @virtual_path, @updated_at, @format, @variant ]
    end

    def marshal_load(array) # :nodoc:
      @source, @identifier, @handler, @compiled, @locals, @virtual_path, @updated_at, @format, @variant = *array
      @compile_mutex = Mutex.new
    end

    private

      # Compile a template. This method ensures a template is compiled
      # just once and removes the source after it is compiled.
      def compile!(view)
        return if @compiled

        # Templates can be used concurrently in threaded environments
        # so compilation and any instance variable modification must
        # be synchronized
        @compile_mutex.synchronize do
          # Any thread holding this lock will be compiling the template needed
          # by the threads waiting. So re-check the @compiled flag to avoid
          # re-compilation
          return if @compiled

          mod = view.compiled_method_container

          instrument("!compile_template") do
            compile(mod)
          end

          # Just discard the source if we have a virtual path. This
          # means we can get the template back.
          @source = nil if @virtual_path
          @compiled = true
        end
      end

      class LegacyTemplate < DelegateClass(Template) # :nodoc:
        attr_reader :source

        def initialize(template, source)
          super(template)
          @source = source
        end
      end

      # Among other things, this method is responsible for properly setting
      # the encoding of the compiled template.
      #
      # If the template engine handles encodings, we send the encoded
      # String to the engine without further processing. This allows
      # the template engine to support additional mechanisms for
      # specifying the encoding. For instance, ERB supports <%# encoding: %>
      #
      # Otherwise, after we figure out the correct encoding, we then
      # encode the source into <tt>Encoding.default_internal</tt>.
      # In general, this means that templates will be UTF-8 inside of Rails,
      # regardless of the original source encoding.
      def compile(mod)
        source = encode!
        code = @handler.call(self, source)

        # Make sure that the resulting String to be eval'd is in the
        # encoding of the code
        source = +<<-end_src
          def #{method_name}(local_assigns, output_buffer)
            @virtual_path = #{@virtual_path.inspect};#{locals_code};#{code}
          end
        end_src

        # Make sure the source is in the encoding of the returned code
        source.force_encoding(code.encoding)

        # In case we get back a String from a handler that is not in
        # BINARY or the default_internal, encode it to the default_internal
        source.encode!

        # Now, validate that the source we got back from the template
        # handler is valid in the default_internal. This is for handlers
        # that handle encoding but screw up
        unless source.valid_encoding?
          raise WrongEncodingError.new(source, Encoding.default_internal)
        end

        mod.module_eval(source, identifier, 0)
      end

      def handle_render_error(view, e)
        if e.is_a?(Template::Error)
          e.sub_template_of(self)
          raise e
        else
          template = self
          unless template.source
            template = refresh(view)
            template.encode!
          end
          raise Template::Error.new(template)
        end
      end

      def locals_code
        # Only locals with valid variable names get set directly. Others will
        # still be available in local_assigns.
        locals = @locals - Module::RUBY_RESERVED_KEYWORDS
        locals = locals.grep(/\A@?(?![A-Z0-9])(?:[[:alnum:]_]|[^\0-\177])+\z/)

        # Assign for the same variable is to suppress unused variable warning
        locals.each_with_object(+"") { |key, code| code << "#{key} = local_assigns[:#{key}]; #{key} = #{key};" }
      end

      def method_name
        @method_name ||= begin
          m = +"_#{identifier_method_name}__#{@identifier.hash}_#{__id__}"
          m.tr!("-", "_")
          m
        end
      end

      def identifier_method_name
        short_identifier.tr("^a-z_", "_")
      end

      def instrument(action, &block) # :doc:
        ActiveSupport::Notifications.instrument("#{action}.action_view", instrument_payload, &block)
      end

      def instrument_render_template(&block)
        ActiveSupport::Notifications.instrument("!render_template.action_view", instrument_payload, &block)
      end

      def instrument_payload
        { virtual_path: @virtual_path, identifier: @identifier }
      end
  end
end
# frozen_string_literal: true

require "active_support/core_ext/object/try"
require "active_support/core_ext/kernel/singleton_class"
require "active_support/deprecation"
require "thread"
require "delegate"

module ActionView
  # = Action View Template
  class Template
    extend ActiveSupport::Autoload

    def self.finalize_compiled_template_methods
      ActiveSupport::Deprecation.warn "ActionView::Template.finalize_compiled_template_methods is deprecated and has no effect"
    end

    def self.finalize_compiled_template_methods=(_)
      ActiveSupport::Deprecation.warn "ActionView::Template.finalize_compiled_template_methods= is deprecated and has no effect"
    end

    # === Encodings in ActionView::Template
    #
    # ActionView::Template is one of a few sources of potential
    # encoding issues in Rails. This is because the source for
    # templates are usually read from disk, and Ruby (like most
    # encoding-aware programming languages) assumes that the
    # String retrieved through File IO is encoded in the
    # <tt>default_external</tt> encoding. In Rails, the default
    # <tt>default_external</tt> encoding is UTF-8.
    #
    # As a result, if a user saves their template as ISO-8859-1
    # (for instance, using a non-Unicode-aware text editor),
    # and uses characters outside of the ASCII range, their
    # users will see diamonds with question marks in them in
    # the browser.
    #
    # For the rest of this documentation, when we say "UTF-8",
    # we mean "UTF-8 or whatever the default_internal encoding
    # is set to". By default, it will be UTF-8.
    #
    # To mitigate this problem, we use a few strategies:
    # 1. If the source is not valid UTF-8, we raise an exception
    #    when the template is compiled to alert the user
    #    to the problem.
    # 2. The user can specify the encoding using Ruby-style
    #    encoding comments in any template engine. If such
    #    a comment is supplied, Rails will apply that encoding
    #    to the resulting compiled source returned by the
    #    template handler.
    # 3. In all cases, we transcode the resulting String to
    #    the UTF-8.
    #
    # This means that other parts of Rails can always assume
    # that templates are encoded in UTF-8, even if the original
    # source of the template was not UTF-8.
    #
    # From a user's perspective, the easiest thing to do is
    # to save your templates as UTF-8. If you do this, you
    # do not need to do anything else for things to "just work".
    #
    # === Instructions for template handlers
    #
    # The easiest thing for you to do is to simply ignore
    # encodings. Rails will hand you the template source
    # as the default_internal (generally UTF-8), raising
    # an exception for the user before sending the template
    # to you if it could not determine the original encoding.
    #
    # For the greatest simplicity, you can support only
    # UTF-8 as the <tt>default_internal</tt>. This means
    # that from the perspective of your handler, the
    # entire pipeline is just UTF-8.
    #
    # === Advanced: Handlers with alternate metadata sources
    #
    # If you want to provide an alternate mechanism for
    # specifying encodings (like ERB does via <%# encoding: ... %>),
    # you may indicate that you will handle encodings yourself
    # by implementing <tt>handles_encoding?</tt> on your handler.
    #
    # If you do, Rails will not try to encode the String
    # into the default_internal, passing you the unaltered
    # bytes tagged with the assumed encoding (from
    # default_external).
    #
    # In this case, make sure you return a String from
    # your handler encoded in the default_internal. Since
    # you are handling out-of-band metadata, you are
    # also responsible for alerting the user to any
    # problems with converting the user's data to
    # the <tt>default_internal</tt>.
    #
    # To do so, simply raise +WrongEncodingError+ as follows:
    #
    #     raise WrongEncodingError.new(
    #       problematic_string,
    #       expected_encoding
    #     )

    ##
    # :method: local_assigns
    #
    # Returns a hash with the defined local variables.
    #
    # Given this sub template rendering:
    #
    #   <%= render "shared/header", { headline: "Welcome", person: person } %>
    #
    # You can use +local_assigns+ in the sub templates to access the local variables:
    #
    #   local_assigns[:headline] # => "Welcome"

    eager_autoload do
      autoload :Error
      autoload :File
      autoload :Handlers
      autoload :HTML
      autoload :Inline
      autoload :Text
      autoload :Types
    end

    extend Template::Handlers

    attr_reader :source, :identifier, :handler, :original_encoding, :updated_at
    attr_reader :variable, :format, :variant, :locals, :virtual_path

    def initialize(source, identifier, handler, format: nil, variant: nil, locals: nil, virtual_path: nil, updated_at: nil)
      unless locals
        ActiveSupport::Deprecation.warn "ActionView::Template#initialize requires a locals parameter"
        locals = []
      end

      @source            = source
      @identifier        = identifier
      @handler           = handler
      @compiled          = false
      @locals            = locals
      @virtual_path      = virtual_path

      @variable = if @virtual_path
        base = @virtual_path[-1] == "/" ? "" : ::File.basename(@virtual_path)
        base =~ /\A_?(.*?)(?:\.\w+)*\z/
        $1.to_sym
      end

      if updated_at
        ActiveSupport::Deprecation.warn "ActionView::Template#updated_at is deprecated"
        @updated_at        = updated_at
      else
        @updated_at        = Time.now
      end
      @format            = format
      @variant           = variant
      @compile_mutex     = Mutex.new
    end

    deprecate :original_encoding
    deprecate :updated_at
    deprecate def virtual_path=(_); end
    deprecate def locals=(_); end
    deprecate def formats=(_); end
    deprecate def formats; Array(format); end
    deprecate def variants=(_); end
    deprecate def variants; [variant]; end

    # Returns whether the underlying handler supports streaming. If so,
    # a streaming buffer *may* be passed when it starts rendering.
    def supports_streaming?
      handler.respond_to?(:supports_streaming?) && handler.supports_streaming?
    end

    # Render a template. If the template was not compiled yet, it is done
    # exactly before rendering.
    #
    # This method is instrumented as "!render_template.action_view". Notice that
    # we use a bang in this instrumentation because you don't want to
    # consume this in production. This is only slow if it's being listened to.
    def render(view, locals, buffer = ActionView::OutputBuffer.new, &block)
      instrument_render_template do
        compile!(view)
        view._run(method_name, self, locals, buffer, &block)
      end
    rescue => e
      handle_render_error(view, e)
    end

    def type
      @type ||= Types[format]
    end

    # Receives a view object and return a template similar to self by using @virtual_path.
    #
    # This method is useful if you have a template object but it does not contain its source
    # anymore since it was already compiled. In such cases, all you need to do is to call
    # refresh passing in the view object.
    #
    # Notice this method raises an error if the template to be refreshed does not have a
    # virtual path set (true just for inline templates).
    def refresh(view)
      raise "A template needs to have a virtual path in order to be refreshed" unless @virtual_path
      lookup  = view.lookup_context
      pieces  = @virtual_path.split("/")
      name    = pieces.pop
      partial = !!name.sub!(/^_/, "")
      lookup.disable_cache do
        lookup.find_template(name, [ pieces.join("/") ], partial, @locals)
      end
    end

    def short_identifier
      @short_identifier ||= defined?(Rails.root) ? identifier.sub("#{Rails.root}/", "") : identifier
    end

    def inspect
      "#<#{self.class.name} #{short_identifier} locals=#{@locals.inspect}>"
    end

    # This method is responsible for properly setting the encoding of the
    # source. Until this point, we assume that the source is BINARY data.
    # If no additional information is supplied, we assume the encoding is
    # the same as <tt>Encoding.default_external</tt>.
    #
    # The user can also specify the encoding via a comment on the first
    # line of the template (# encoding: NAME-OF-ENCODING). This will work
    # with any template engine, as we process out the encoding comment
    # before passing the source on to the template engine, leaving a
    # blank line in its stead.
    def encode!
      source = self.source

      return source unless source.encoding == Encoding::BINARY

      # Look for # encoding: *. If we find one, we'll encode the
      # String in that encoding, otherwise, we'll use the
      # default external encoding.
      if source.sub!(/\A#{ENCODING_FLAG}/, "")
        encoding = magic_encoding = $1
      else
        encoding = Encoding.default_external
      end

      # Tag the source with the default external encoding
      # or the encoding specified in the file
      source.force_encoding(encoding)

      # If the user didn't specify an encoding, and the handler
      # handles encodings, we simply pass the String as is to
      # the handler (with the default_external tag)
      if !magic_encoding && @handler.respond_to?(:handles_encoding?) && @handler.handles_encoding?
        source
      # Otherwise, if the String is valid in the encoding,
      # encode immediately to default_internal. This means
      # that if a handler doesn't handle encodings, it will
      # always get Strings in the default_internal
      elsif source.valid_encoding?
        source.encode!
      # Otherwise, since the String is invalid in the encoding
      # specified, raise an exception
      else
        raise WrongEncodingError.new(source, encoding)
      end
    end


    # Exceptions are marshalled when using the parallel test runner with DRb, so we need
    # to ensure that references to the template object can be marshalled as well. This means forgoing
    # the marshalling of the compiler mutex and instantiating that again on unmarshalling.
    def marshal_dump # :nodoc:
      [ @source, @identifier, @handler, @compiled, @locals, @virtual_path, @updated_at, @format, @variant ]
    end

    def marshal_load(array) # :nodoc:
      @source, @identifier, @handler, @compiled, @locals, @virtual_path, @updated_at, @format, @variant = *array
      @compile_mutex = Mutex.new
    end

    private

      # Compile a template. This method ensures a template is compiled
      # just once and removes the source after it is compiled.
      def compile!(view)
        return if @compiled

        # Templates can be used concurrently in threaded environments
        # so compilation and any instance variable modification must
        # be synchronized
        @compile_mutex.synchronize do
          # Any thread holding this lock will be compiling the template needed
          # by the threads waiting. So re-check the @compiled flag to avoid
          # re-compilation
          return if @compiled

          mod = view.compiled_method_container

          instrument("!compile_template") do
            compile(mod)
          end

          # Just discard the source if we have a virtual path. This
          # means we can get the template back.
          @source = nil if @virtual_path
          @compiled = true
        end
      end

      class LegacyTemplate < DelegateClass(Template) # :nodoc:
        attr_reader :source

        def initialize(template, source)
          super(template)
          @source = source
        end
      end

      # Among other things, this method is responsible for properly setting
      # the encoding of the compiled template.
      #
      # If the template engine handles encodings, we send the encoded
      # String to the engine without further processing. This allows
      # the template engine to support additional mechanisms for
      # specifying the encoding. For instance, ERB supports <%# encoding: %>
      #
      # Otherwise, after we figure out the correct encoding, we then
      # encode the source into <tt>Encoding.default_internal</tt>.
      # In general, this means that templates will be UTF-8 inside of Rails,
      # regardless of the original source encoding.
      def compile(mod)
        source = encode!
        code = @handler.call(self, source)

        # Make sure that the resulting String to be eval'd is in the
        # encoding of the code
        source = +<<-end_src
          def #{method_name}(local_assigns, output_buffer)
            @virtual_path = #{@virtual_path.inspect};#{locals_code};#{code}
          end
        end_src

        # Make sure the source is in the encoding of the returned code
        source.force_encoding(code.encoding)

        # In case we get back a String from a handler that is not in
        # BINARY or the default_internal, encode it to the default_internal
        source.encode!

        # Now, validate that the source we got back from the template
        # handler is valid in the default_internal. This is for handlers
        # that handle encoding but screw up
        unless source.valid_encoding?
          raise WrongEncodingError.new(source, Encoding.default_internal)
        end

        mod.module_eval(source, identifier, 0)
      end

      def handle_render_error(view, e)
        if e.is_a?(Template::Error)
          e.sub_template_of(self)
          raise e
        else
          template = self
          unless template.source
            template = refresh(view)
            template.encode!
          end
          raise Template::Error.new(template)
        end
      end

      def locals_code
        # Only locals with valid variable names get set directly. Others will
        # still be available in local_assigns.
        locals = @locals - Module::RUBY_RESERVED_KEYWORDS
        locals = locals.grep(/\A@?(?![A-Z0-9])(?:[[:alnum:]_]|[^\0-\177])+\z/)

        # Assign for the same variable is to suppress unused variable warning
        locals.each_with_object(+"") { |key, code| code << "#{key} = local_assigns[:#{key}]; #{key} = #{key};" }
      end

      def method_name
        @method_name ||= begin
          m = +"_#{identifier_method_name}__#{@identifier.hash}_#{__id__}"
          m.tr!("-", "_")
          m
        end
      end

      def identifier_method_name
        short_identifier.tr("^a-z_", "_")
      end

      def instrument(action, &block) # :doc:
        ActiveSupport::Notifications.instrument("#{action}.action_view", instrument_payload, &block)
      end

      def instrument_render_template(&block)
        ActiveSupport::Notifications.instrument("!render_template.action_view", instrument_payload, &block)
      end

      def instrument_payload
        { virtual_path: @virtual_path, identifier: @identifier }
      end
  end
end