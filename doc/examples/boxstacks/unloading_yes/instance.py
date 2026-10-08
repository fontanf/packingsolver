import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPackingWithLeftovers)
instance_builder.set_unloading_constraint(psbs.UnloadingConstraint.OnlyXMovements)
instance_builder.add_bin_type(7500, 2400, 3000, copies=2)
instance_builder.add_item_type(3000, 1400, 1000, stackability_id=0)
instance_builder.add_item_type(3000, 2400, 1000, group_id=1, stackability_id=1)
instance_builder.add_item_type(2500, 1000, 1000, group_id=2, stackability_id=2)
instance = instance_builder.build()
