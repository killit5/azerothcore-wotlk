SET
@Entry = 190011,
@Name = "Thaumaturge Vashreen";

INSERT INTO `creature_template` (`entry`, `modelid1`, `modelid2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `scale`, `rank`, `dmgschool`, `baseattacktime`, `rangeattacktime`, `unit_class`, `unit_flags`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `AIName`, `MovementType`, `HoverHeight`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `flags_extra`, `ScriptName`) VALUES
(@Entry, 20988, 0, @Name, "Arcane Reforger", NULL, 0, 80, 80, 2, 35, 1, 1, 0, 0, 2000, 0, 1, 0, 7, 138936390, 0, 0, 0, '', 0, 1, 0, 0, 1, 0, 0, 'npc_reforger');

INSERT INTO `acore_string` VALUES (40000, 'Spirit', NULL, 'Esprit', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40001, 'Dodge rating', NULL, 'Score d\'esquive', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40002, 'Parry rating', NULL, 'Score de parade', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40003, 'Hit rating', NULL, 'Score de toucher', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40004, 'Crit rating', NULL, 'Score de critique', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40005, 'Haste rating', NULL, 'Score de hâte', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40006, 'Expertise rating', NULL, 'Score d\'experrtise', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40007, 'Head', NULL, 'Tête', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40008, 'Neck', NULL, 'Cou', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40009, 'Shoulders', NULL, 'Épaules', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40010, 'Shirt', NULL, 'Chemise', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40011, 'Chest', NULL, 'Torse', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40012, 'Waist', NULL, 'Ceinture', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40013, 'Legs', NULL, 'Jambes', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40014, 'Feet', NULL, 'Pieds', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40015, 'Wrists', NULL, 'Brassards', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40016, 'Hands', NULL, 'Gants', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40017, 'Right finger', NULL, 'Bague droite', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40018, 'Left finger', NULL, 'Bague gauche', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40019, 'Right trinket', NULL, 'Bijou droite', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40020, 'Left trinket', NULL, 'Bijou gauche', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40021, 'Back', NULL, 'Cape', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40022, 'Main hand', NULL, 'Main droite', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40023, 'Off hand', NULL, 'Main gauche', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40024, 'Tabard', NULL, 'Tabard', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40025, 'Ranged', NULL, 'Distance', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40028, 'Back..', NULL, 'Retour..', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40026, 'Invalid item selected', NULL, 'Item sélectionné invalide', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40027, 'Select slot to remove reforge from:', NULL, 'Sélectionner l\'emplacement où supprimer la reforge', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40029, 'Remove reforge from ', NULL, 'Supprimer reforge de ', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40030, 'Update menu', NULL, 'Mise à jour menu', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40031, 'Not enough money', NULL, 'Pas assez d\'argent', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40032, 'Are you sure you want to reforge ', NULL, 'Étes-vous sure de vouloir reforge ', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40033, 'Stat to increase:', NULL, 'Stat à augmenter:', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40034, 'Stat to decrease:', NULL, 'Stat à diminuer:', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40035, 'Remove reforges', NULL, 'Supprimer reforges', NULL, NULL, NULL, NULL, NULL, NULL);
INSERT INTO `acore_string` VALUES (40036, 'Select slot of the item to reforge:', NULL, 'Sélectionner l\'emplacement de l\'item à reforge:', NULL, NULL, NULL, NULL, NULL, NULL);