import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
bin_type_id = instance_builder.add_bin_type(10, 12)
instance_builder.add_defect(bin_type_id, 4, 4, 2, 2)
instance_builder.add_bin_type(10, 12, copies=2)
instance_builder.add_item_type(10, 6, oriented=True, copies=2)
instance = instance_builder.build()
