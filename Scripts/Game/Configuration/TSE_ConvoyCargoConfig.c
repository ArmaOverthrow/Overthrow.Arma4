[BaseContainerProps(configRoot : true)]
class TSE_ConvoyCargoConfig : ScriptAndConfig
{
    [Attribute("50", UIWidgets.EditBox, desc: "Шанс спавна оружия (0-100)")]
    int m_iWeaponsChance;
    
    [Attribute("2", UIWidgets.EditBox, desc: "Минимальное количество оружия")]
    int m_iWeaponsMinCount;
    
    [Attribute("5", UIWidgets.EditBox, desc: "Максимальное количество оружия")]
    int m_iWeaponsMaxCount;
    
    [Attribute("70", UIWidgets.EditBox, desc: "Шанс спавна патронов (0-100)")]
    int m_iAmmunitionChance;
    
    [Attribute("5", UIWidgets.EditBox, desc: "Минимальное количество патронов")]
    int m_iAmmunitionMinCount;
    
    [Attribute("15", UIWidgets.EditBox, desc: "Максимальное количество патронов")]
    int m_iAmmunitionMaxCount;
    
    [Attribute("30", UIWidgets.EditBox, desc: "Шанс спавна обвесов (0-100)")]
    int m_iAttachmentsChance;
    
    [Attribute("1", UIWidgets.EditBox, desc: "Минимальное количество обвесов")]
    int m_iAttachmentsMinCount;
    
    [Attribute("4", UIWidgets.EditBox, desc: "Максимальное количество обвесов")]
    int m_iAttachmentsMaxCount;
    
    [Attribute("25", UIWidgets.EditBox, desc: "Шанс спавна гранат (0-100)")]
    int m_iThrowableChance;
    
    [Attribute("2", UIWidgets.EditBox, desc: "Минимальное количество гранат")]
    int m_iThrowableMinCount;
    
    [Attribute("6", UIWidgets.EditBox, desc: "Максимальное количество гранат")]
    int m_iThrowableMaxCount;
    
    [Attribute("20", UIWidgets.EditBox, desc: "Шанс спавна взрывчатки (0-100)")]
    int m_iExplosivesChance;
    
    [Attribute("1", UIWidgets.EditBox, desc: "Минимальное количество взрывчатки")]
    int m_iExplosivesMinCount;
    
    [Attribute("3", UIWidgets.EditBox, desc: "Максимальное количество взрывчатки")]
    int m_iExplosivesMaxCount;
} 