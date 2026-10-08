import packingsolver.box as psb

instance_builder = psb.InstanceBuilder()
instance_builder.set_objective(psb.Objective.BinPacking)
instance_builder.add_bin_type(20, 20, 10, maximum_weight=200, copies=10)
instance_builder.add_item_type(10, 10, 10, weight=100, copies=4)
instance = instance_builder.build()
