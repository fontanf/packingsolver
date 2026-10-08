import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.BinPacking)
instance_builder.add_bin_type(600, 400, maximum_weight=200, copies=10)
instance_builder.add_item_type(300, 200, weight=100, copies=4)
instance = instance_builder.build()
