import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.BinPackingWithLeftovers)
instance_builder.add_bin_type(1000, 500, copies=10)
instance_builder.add_item_type(300, 200, copies=10)
instance_builder.add_item_type(250, 150, copies=10)
instance = instance_builder.build()
