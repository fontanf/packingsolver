import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.OpenDimensionX)
instance_builder.set_unloading_constraint(psr.UnloadingConstraint.IncreasingX)
instance_builder.add_bin_type(800, 400)
instance_builder.add_item_type(500, 200)
instance_builder.add_item_type(200, 200, group_id=1)
instance_builder.add_item_type(150, 150, group_id=1)
instance = instance_builder.build()
