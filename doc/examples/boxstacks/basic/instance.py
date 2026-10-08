import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.Knapsack)
instance_builder.add_bin_type(7500, 2400, 3000)
instance_builder.add_item_type(2500, 800, 750, stackability_id=0, copies=10)
instance_builder.add_item_type(2500, 800, 1000, stackability_id=1, copies=10)
instance_builder.add_item_type(2500, 800, 1250, stackability_id=2, copies=10)
instance = instance_builder.build()
