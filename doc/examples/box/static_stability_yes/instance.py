import packingsolver.box as psb

instance_builder = psb.InstanceBuilder()
instance_builder.set_objective(psb.Objective.BinPacking)
instance_builder.set_static_stability(True)
instance_builder.add_bin_type(10, 10, 20, copies=10)
instance_builder.add_item_type(9, 3, 12, copies=2)
instance_builder.add_item_type(10, 7, 6, copies=2)
instance = instance_builder.build()
