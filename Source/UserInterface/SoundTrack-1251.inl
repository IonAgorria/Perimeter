    template<class Archive>
    void serialize(Archive& ar) {
		ar & WRAP_NAME(fileName_, "fileName");
		ar & TRANSLATE_OBJECT(fileNames, "����� ������");
		ar & TRANSLATE_OBJECT(randomChoice, "��������� �����");
		if(ar.isInput() && fileNames.empty() && !fileName_.empty())
			fileNames.push_back(fileName_);
	}