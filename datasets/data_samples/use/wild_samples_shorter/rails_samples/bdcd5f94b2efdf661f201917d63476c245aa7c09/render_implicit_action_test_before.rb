      "render_implicit_action/simple/hello_world.html.erb"     => "Hello world!",
      "render_implicit_action/simple/hyphen-ated.html.erb"     => "Hello hyphen-ated!",
      "render_implicit_action/simple/not_implemented.html.erb" => "Not Implemented"
    )]

    def hello_world() end
  end

      assert_status 200
    end

    test "available_action? returns true for implicit actions" do
      assert SimpleController.new.available_action?(:hello_world)
      assert SimpleController.new.available_action?(:"hyphen-ated")
      assert SimpleController.new.available_action?(:not_implemented)
    end
  end
end