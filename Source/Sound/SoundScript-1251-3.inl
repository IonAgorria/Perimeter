	template<class Archive>
	void serialize(Archive& ar) {
		ar & TRANSLATE_OBJECT(name, "&name");
		ar & TRANSLATE_OBJECT(is3D, "����� ���������");
		ar & TRANSLATE_OBJECT(data, "�����");
	}
