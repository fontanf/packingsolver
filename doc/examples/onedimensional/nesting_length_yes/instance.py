import packingsolver.onedimensional as pso

instance_builder = pso.InstanceBuilder()
instance_builder.set_objective(pso.Objective.BinPacking)
instance_builder.add_bin_type(500, copies=10)
instance_builder.add_item_type(70, nesting_length=10, copies=8)
instance = instance_builder.build()
