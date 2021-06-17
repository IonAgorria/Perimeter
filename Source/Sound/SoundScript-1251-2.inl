	template<class Archive>
	void serialize(Archive& ar) {
		ar & TRANSLATE_OBJECT(name, "&name");
		ar & TRANSLATE_OBJECT(volume, "��������� ����� 0..1 (�� ���������: 1)");
		ar & TRANSLATE_OBJECT(volmin, "����������� ��������� ����� 0..1 (�� ���������: 0)");
	
		ar & TRANSLATE_OBJECT(language_dependency, "���� ������ ��� ������ ������");
		ar & TRANSLATE_OBJECT(belligerent_dependency, "���� ������ ��� ������ ������� ������");

		ar & TRANSLATE_OBJECT(soundName, "�������� �����");
		ar & TRANSLATE_OBJECT(additionalNames, "�������������� ����� ��� RND");

		ar & TRANSLATE_OBJECT(radius, "�������� ������� (������ � 3D) � ����� ������� ��������� ����� �������� ������� (�� ���������: 50)");
		ar & TRANSLATE_OBJECT(max_radius, "������������ ���������� �� �������, ����� �������� ���� �������� ���� �������� (�� ���������: oo)");

		ar & TRANSLATE_OBJECT(max_num_sound, "������������ ���������� ������������ �������� ������ (�� ���������: 5)");

		ar & TRANSLATE_OBJECT(delta, "������");
	}
