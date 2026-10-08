import packingsolver.box as psb

instance_builder = psb.InstanceBuilder()
instance_builder.set_objective(psb.Objective.Knapsack)
instance_builder.add_bin_type(216, 173, 110)
instance_builder.add_item_type(108, 76, 30, copies=20)
instance_builder.add_item_type(110, 43, 25, copies=20)
instance_builder.add_item_type(92, 81, 55, copies=20)
instance = instance_builder.build()
