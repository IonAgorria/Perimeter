template<class Archive>
void serialize(Archive& ar) {
    ar & TRANSLATE_OBJECT(enable, "&��������");
    ar & TRANSLATE_OBJECT(random, "�������� ������� ����� �������� ��� ��� �������");
    ar & TRANSLATE_OBJECT(up, "�� ������� ��� ������� ����� ����� ������������� (0..2)");
    ar & TRANSLATE_OBJECT(down, "�� ������� ��� ������� ����� ����� ����������� (0..20)");
}
