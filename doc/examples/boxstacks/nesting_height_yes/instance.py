import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPacking)
instance_builder.add_bin_type(7500, 2400, 3000, copies=4)
instance_builder.add_item_type(2000, 1000, 1600, nesting_height=200, copies=6)
instance_builder.add_item_type(2000, 1000, 1600, nesting_height=200, copies=6)
instance = instance_builder.build()
