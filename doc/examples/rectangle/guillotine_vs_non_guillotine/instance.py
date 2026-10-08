import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.BinPacking)
instance_builder.add_bin_type(12, 9, copies=2)
instance_builder.add_item_type(8, 3, oriented=True)
instance_builder.add_item_type(4, 5, oriented=True)
instance_builder.add_item_type(5, 4, oriented=True)
instance_builder.add_item_type(7, 6, oriented=True)
instance = instance_builder.build()
