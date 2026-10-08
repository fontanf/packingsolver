import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPacking)
instance_builder.add_bin_type(13500, 2440, 2900, copies=2)
instance_builder.add_item_type(1200, 1000, 1200, weight=1200, copies=19)
instance_builder.add_item_type(1200, 1000, 800, weight=800, copies=19)
instance = instance_builder.build()
