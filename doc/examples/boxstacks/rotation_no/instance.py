import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPacking)
instance_builder.add_bin_type(7500, 2400, 3000, copies=2)
instance_builder.add_item_type(2400, 1250, 1000, copies=18)
instance = instance_builder.build()
