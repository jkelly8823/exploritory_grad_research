          value = value.present? ? Array(value) : default_#{name}
          _set_detail(:#{name}, value) if value != @details[:#{name}]
        end

        remove_possible_method :initialize_details
        def initialize_details(details)
          #{initialize.join("\n")}
        end
      METHOD
    end

    # Holds accessors for the registered details.
    module Accessors #:nodoc:
    end

    register_detail(:locale) do
      locales = [I18n.locale]
      locales.concat(I18n.fallbacks[I18n.locale]) if I18n.respond_to? :fallbacks
      locales << I18n.default_locale
      locales.uniq!
      locales
    end
    register_detail(:formats) { ActionView::Base.default_formats || [:html, :text, :js, :css,  :xml, :json] }
    register_detail(:variants) { [] }
    register_detail(:handlers) { Template::Handlers.extensions }

    class DetailsKey #:nodoc:
      alias :eql? :equal?
      alias :object_hash :hash

      attr_reader :hash
      @details_keys = Concurrent::Map.new

      def self.get(details)
        if details[:formats]
          details = details.dup
          details[:formats] &= Mime::SET.symbols
        end
        @details_keys[details] ||= new
      end

      def self.clear
        @details_keys.clear
      end

      def initialize
        @hash = object_hash
      end
    end

    # Add caching behavior on top of Details.
    module DetailsCache
      attr_accessor :cache

      # Calculate the details key. Remove the handlers from calculation to improve performance
      # since the user cannot modify it explicitly.
      def details_key #:nodoc:
        @details_key ||= DetailsKey.get(@details) if @cache
      end

      # Temporary skip passing the details_key forward.
      def disable_cache
        old_value, @cache = @cache, false
        yield
      ensure
        @cache = old_value
      end

    protected

      def _set_detail(key, value)
        @details = @details.dup if @details_key
        @details_key = nil
        @details[key] = value
      end
    end

    # Helpers related to template lookup using the lookup context information.
    module ViewPaths
      attr_reader :view_paths, :html_fallback_for_js

      # Whenever setting view paths, makes a copy so that we can manipulate them in
      # instance objects as we wish.
      def view_paths=(paths)
        @view_paths = ActionView::PathSet.new(Array(paths))
      end

      def find(name, prefixes = [], partial = false, keys = [], options = {})
        @view_paths.find(*args_for_lookup(name, prefixes, partial, keys, options))
      end
      alias :find_template :find

      def find_file(name, prefixes = [], partial = false, keys = [], options = {})
        @view_paths.find_file(*args_for_lookup(name, prefixes, partial, keys, options))
      end

      def find_all(name, prefixes = [], partial = false, keys = [], options = {})
        @view_paths.find_all(*args_for_lookup(name, prefixes, partial, keys, options))
      end

      def exists?(name, prefixes = [], partial = false, keys = [], **options)
        @view_paths.exists?(*args_for_lookup(name, prefixes, partial, keys, options))
      end
      alias :template_exists? :exists?

      # Adds fallbacks to the view paths. Useful in cases when you are rendering
      # a :file.
      def with_fallbacks
        added_resolvers = 0
        self.class.fallbacks.each do |resolver|
          next if view_paths.include?(resolver)
          view_paths.push(resolver)
          added_resolvers += 1
        end
        yield
      ensure
        added_resolvers.times { view_paths.pop }
      end

    protected

      def args_for_lookup(name, prefixes, partial, keys, details_options) #:nodoc:
        name, prefixes = normalize_name(name, prefixes)
        details, details_key = detail_args_for(details_options)
        [name, prefixes, partial || false, details, details_key, keys]
      end

      # Compute details hash and key according to user options (e.g. passed from #render).
      def detail_args_for(options)
        return @details, details_key if options.empty? # most common path.
        user_details = @details.merge(options)

        if @cache
          details_key = DetailsKey.get(user_details)
        else
          details_key = nil
        end

        [user_details, details_key]
      end

      # Support legacy foo.erb names even though we now ignore .erb
      # as well as incorrectly putting part of the path in the template
      # name instead of the prefix.
      def normalize_name(name, prefixes) #:nodoc:
        prefixes = prefixes.presence
        parts    = name.to_s.split('/'.freeze)
        parts.shift if parts.first.empty?
        name     = parts.pop

        return name, prefixes || [""] if parts.empty?

        parts    = parts.join('/'.freeze)
        prefixes = prefixes ? prefixes.map { |p| "#{p}/#{parts}" } : [parts]

        return name, prefixes
      end
    end

    include Accessors
    include DetailsCache
    include ViewPaths

    def initialize(view_paths, details = {}, prefixes = [])
      @details, @details_key = {}, nil
      @cache = true
      @prefixes = prefixes
      @rendered_format = nil

      self.view_paths = view_paths
      initialize_details(details)
    end

    # Override formats= to expand ["*/*"] values and automatically
    # add :html as fallback to :js.
    def formats=(values)
      if values
        values.concat(default_formats) if values.delete "*/*".freeze
        if values == [:js]
          values << :html
          @html_fallback_for_js = true
        end
      end
      super(values)
    end

    # Override locale to return a symbol instead of array.
    def locale
      @details[:locale].first
    end

    # Overload locale= to also set the I18n.locale. If the current I18n.config object responds
    # to original_config, it means that it has a copy of the original I18n configuration and it's
    # acting as proxy, which we need to skip.
    def locale=(value)
      if value
        config = I18n.config.respond_to?(:original_config) ? I18n.config.original_config : I18n.config
        config.locale = value
      end

      super(default_locale)
    end
  end
end