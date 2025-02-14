    # has_destroy_flag? or if a <tt>:reject_if</tt> proc exists for this
    # association and evaluates to +true+.
    def reject_new_record?(association_name, attributes)
      has_destroy_flag?(attributes) || call_reject_if(association_name, attributes)
    end

    # Determines if a record with the particular +attributes+ should be
    # rejected by calling the reject_if Symbol or Proc (if defined).
    #
    # Returns false if there is a +destroy_flag+ on the attributes.
    def call_reject_if(association_name, attributes)
      return false if has_destroy_flag?(attributes)
      case callback = self.nested_attributes_options[association_name][:reject_if]
      when Symbol
        method(callback).arity == 0 ? send(callback) : send(callback, attributes)
      when Proc
      end
    end

    def raise_nested_attributes_record_not_found!(association_name, record_id)
      model = self.class._reflect_on_association(association_name).klass.name
      raise RecordNotFound.new("Couldn't find #{model} with ID=#{record_id} for #{self.class.name} with ID=#{id}",
                               model, 'id', record_id)