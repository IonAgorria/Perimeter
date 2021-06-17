	template<class Archive>
	void serialize(Archive& ar) {
		ar & WRAP_OBJECT(version);
		ar & TRANSLATE_OBJECT(worldName, "��� ����");
		ar & TRANSLATE_NAME(missionDescriptionID, "missionDescription", "�������� ������");
		ar & TRANSLATE_OBJECT(difficulty, "������� ���������");
		ar & TRANSLATE_OBJECT(playersData, "������");
		ar & WRAP_OBJECT(missionNumber);
		
		ar & TRANSLATE_OBJECT(playerAmountScenarioMax, "������������ ���������� �������");
		ar & WRAP_OBJECT(playersShufflingIndices);
		ar & WRAP_OBJECT(activePlayerID);
		
		ar & WRAP_OBJECT(globalTime);
		ar & WRAP_OBJECT(gameSpeed);
		ar & WRAP_OBJECT(gamePaused);

		ar & WRAP_OBJECT(originalSaveName);
	}
