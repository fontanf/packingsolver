import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.add_bin_type(10, 10, copies=4)
instance_builder.add_item_type(5, 6, oriented=True, stack_id=0)
instance_builder.add_item_type(5, 7, oriented=True, stack_id=0)
instance_builder.add_item_type(5, 3, oriented=True, stack_id=1)
instance_builder.add_item_type(5, 4, oriented=True, stack_id=1)
instance = instance_builder.build()
