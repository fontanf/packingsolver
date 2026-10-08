import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.VariableSizedBinPacking)
instance_builder.add_bin_type(1000, 500, copies=10)
instance_builder.add_bin_type(700, 400, copies=10)
instance_builder.add_item_type(250, 150, copies=10)
instance_builder.add_item_type(200, 100, copies=12)
instance_builder.add_item_type(175, 75, copies=12)
instance = instance_builder.build()
