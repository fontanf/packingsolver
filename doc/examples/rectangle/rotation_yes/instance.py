import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.BinPackingWithLeftovers)
instance_builder.add_bin_type(700, 500, copies=2)
instance_builder.add_item_type(400, 200, copies=3)
instance = instance_builder.build()
