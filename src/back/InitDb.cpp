#include "InitDb.h"
#include "UtilDb.h"

namespace back {

  InitDb::InitDb(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
      : txn_(txn)
      , pg_(pg)
      , schema_(schema)
  {}

  // Column definition for the folder table, shared by both creation paths.
  static constexpr const char *FOLDER_TABLE_DEF = "(id varchar(32) PRIMARY KEY, parent_id varchar(32), name text)";

  void InitDb::init_folder_table() const
  {
    const std::string schemaQuote = pg_.quote_name(schema_);

    txn_.exec("CREATE TABLE IF NOT EXISTS " + schemaQuote + ".folder " + FOLDER_TABLE_DEF);

    ensureCreatedAt(txn_, schema_, "folder");
    ensureLastModifiedAt(txn_, schema_, "folder");
  }

  void InitDb::init_unit_table() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);

    txn_.exec("CREATE TABLE IF NOT EXISTS " + schemaQuoted + ".unit " +
              "(id varchar(32) PRIMARY KEY,"
              " parent_folder_id varchar(32),"
              " name text,"
              " type text CHECK (type IN ('Class','Interface','Enum'))"
              ")");

    txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".unit         IS 'Юнит - один из: Класс, Интерфейс, Перечисление'");
    txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit.id      IS 'Идентификатор юнита'");
    txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit.name    IS 'Наименование юнита'");
    txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit.type    IS 'Тип юнита: Класс, Интерфейс, Перечисление'");

    ensureCreatedAt(txn_, schema_, "unit");
    ensureLastModifiedAt(txn_, schema_, "unit");

    init_unit_b_tables();
  }

  void InitDb::init_repo_schema() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);

    txn_.exec("CREATE SCHEMA IF NOT EXISTS " + schemaQuoted);
    txn_.exec("CREATE TABLE IF NOT EXISTS " + schemaQuoted +
              ".lang_setting "
              "(name varchar(150) PRIMARY KEY, value text)");

    txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".lang_setting         IS 'Настройки данного репозитория'");
    txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".lang_setting.name    IS 'Имя настройки'");
    txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".lang_setting.value   IS 'Текстовое представление данной настройки'");

    ensureCreatedAt(txn_, schema_, "lang_setting");
    ensureLastModifiedAt(txn_, schema_, "lang_setting");

    init_folder_table();
    init_unit_table();
    init_unit_b_tables();
    init_unit_e_tables();
    init_undo_tables();
  }

  void InitDb::init_unit_b_tables() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);

    if (!hasSchema(txn_, schema_)) {
      txn_.exec("CREATE SCHEMA " + schemaQuoted);
    }

    if (!hasTable(txn_, schema_, "unit_b")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b ("
                "  id        VARCHAR(32) primary key,"
                "  unit_id   VARCHAR(32) not null,"
                "  type      VARCHAR(150) not null,"
                "  disabled  BOOL default false,"
                "  x         FLOAT4 not null,"
                "  y         FLOAT4 not null,"
                "  width     FLOAT4 not null,"
                "  height    FLOAT4 not null,"
                "  geom GEOMETRY(Polygon, 0)"
                "  GENERATED ALWAYS AS ("
                "    ST_MakeEnvelope(x, y, x + width, y + height, 0)"
                "  ) STORED"
                ")");

      txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".unit_b          IS 'Базовая структура всех блоков'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.unit_id  IS 'Идентификатор юнита, которому принадлежит данный блок'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.type     IS 'Тип данного блока - соответствует перечислению BlockType'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.disabled IS 'Признак отключения данного блока'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.x        IS 'x координата расположения блока'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.y        IS 'y координата расположения блока'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.width    IS 'Ширина этой части блока и всех его внутренних частей'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.height   IS 'Высота этой части блока и всех его внутренних частей'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b.geom     IS 'Геометрия данной части блока для индексирования'");
    }

    if (!hasIndex(txn_, schema_, "unit_b_geom")) {
      txn_.exec("CREATE INDEX unit_b_geom ON " + schemaQuoted + ".unit_b USING GIST(geom)");
    }

    ensureCreatedAt(txn_, schema_, "unit_b");
    ensureLastModifiedAt(txn_, schema_, "unit_b");

    if (!hasTable(txn_, schema_, "unit_b_method")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b_method "
                "("
                "  id            varchar(32) primary key,"
                "  type          text CHECK (type IN ('Inner','Static','Constructor','Destructor')) default 'Inner',"
                "  access        text CHECK (access IN ('Public','Protected','Private')) default 'Private',"
                "  next_block_id varchar(32),"
                "  name          text"
                ")");

      txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".unit_b_method               IS 'Блок метода'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method.type          IS 'Тип метода - соответствует перечислению MethodType'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method.access        IS 'Доступ метода - соответствует перечислению MethodAccess'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method.next_block_id IS 'Идентификатор блока прицепленный снизу'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method.name          IS 'Имя данного метода'");

      ensureCreatedAt(txn_, schema_, "unit_b_method");
      ensureLastModifiedAt(txn_, schema_, "unit_b_method");
    }

    if (!hasTable(txn_, schema_, "unit_b_method_arg")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b_method_arg "
                "("
                "  id              varchar(32) primary key,"
                "  owner_method_id varchar(32) not null,"
                "  order_index     float8 not null,"
                "  name            text"
                ")");

      txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".unit_b_method_arg                 IS 'Аргумент метода'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method_arg.id              IS 'Идентификатор аргумента'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method_arg.owner_method_id IS 'Идентификатор метода данного аргумента'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_method_arg.name            IS 'Имя аргумента'");

      ensureCreatedAt(txn_, schema_, "unit_b_method_arg");
      ensureLastModifiedAt(txn_, schema_, "unit_b_method_arg");
    }

    if (!hasTable(txn_, schema_, "unit_b_field")) {
      txn_.exec("CREATE TABLE " + schemaQuoted +
                ".unit_b_field "
                "("
                "  id           varchar(32) primary key,"
                "  next_unit_id varchar(32),"
                "  size_bytes   int4 not null default 0,"
                "  expr_id      varchar(32),"
                "  expr_id_used bool not null default false,"
                "  expr_x       float4,"
                "  expr_y       float4,"
                "  expr_width   float4,"
                "  expr_height  float4,"
                "  access       text CHECK (access IN ('Public','Protected','Private')) default 'Private',"
                "  name text"
                ")");

      ensureCreatedAt(txn_, schema_, "unit_b_field");
      ensureLastModifiedAt(txn_, schema_, "unit_b_field");

      txn_.exec("COMMENT ON TABLE " + schemaQuoted +
                ".unit_b_field IS 'Поле юнита. Его размер может быть задан на прямую через size_bytes либо через expr_id,"
                " через которую вычисляется тип данного поля во время верификации'");

      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.id           IS 'Идентификатор блока поля'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.next_unit_id IS 'Подвешенный снизу блок'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.size_bytes   IS 'Размер этого поля заданный изначально, или NULL'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.expr_id      IS 'Идентификатор выражения, определяющего тип этого поля'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.expr_id_used IS 'Признак того, что нужно использовать выражение, а не размер'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                ".unit_b_field.expr_x       IS 'x левого верхнего угла прямоугольника выражения, ОТНОСИТЕЛЬНО unit_b.x'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                ".unit_b_field.expr_y       IS 'y левого верхнего угла прямоугольника выражения, ОТНОСИТЕЛЬНО unit_b.y'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.expr_width   IS 'ширина прямоугольника выражения (= unit_e.width)'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.expr_height  IS 'высота прямоугольника выражения (= unit_e.height)'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.access       IS 'Доступ к этому полю'");
      txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_b_field.name         IS 'Наименование данного поля'");
    }
  }

  void InitDb::init_unit_e_tables() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);
    {
      if (!hasTable(txn_, schema_, "unit_e")) {
        txn_.exec("CREATE TABLE " + schemaQuoted +
                  ".unit_e ("
                  "  id        VARCHAR(32) primary key,"
                  "  type      VARCHAR(150) not null,"
                  "  x         FLOAT4 not null,"
                  "  y         FLOAT4 not null,"
                  "  width     FLOAT4 not null,"
                  "  height    FLOAT4 not null,"
                  "  geom GEOMETRY(Polygon, 0)"
                  "  GENERATED ALWAYS AS ("
                  "    ST_MakeEnvelope(x, y, x + width, y + height, 0)"
                  "  ) STORED"
                  ")");

        txn_.exec("COMMENT ON TABLE " + schemaQuoted + ".unit_e         IS 'Базовая структура для всех частей выражений'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.id     IS 'Идентификатор части выражения'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.type   IS 'Тип выражение - соответствует перечислению ExprType'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.x      IS 'x координата расположения выражения'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.y      IS 'y координата расположения выражения'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.width  IS 'Ширина этой части выражения и всех её внутренних частей'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.height IS 'Высота этой части выражения и всех её внутренних частей'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e.geom   IS 'Геометрия данной части выражения для индексирования'");
      }

      if (!hasIndex(txn_, schema_, "unit_b_geom")) {
        txn_.exec("CREATE INDEX unit_e_geom ON " + schemaQuoted + ".unit_b USING GIST(geom)");
      }

      ensureCreatedAt(txn_, schema_, "unit_e");
      ensureLastModifiedAt(txn_, schema_, "unit_e");
    }
    {
      if (!hasTable(txn_, schema_, "unit_e_unit")) {
        txn_.exec("CREATE TABLE " + schemaQuoted +
                  ".unit_e_unit ("
                  "  id      VARCHAR(32) primary key,"
                  "  unit_id VARCHAR(32)"
                  ")");

        txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".unit_e_unit         IS 'Выражение указывающее на юнит. Вычисляется при верификации'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e_unit.id      IS 'Идентификатор части выражения'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".unit_e_unit.unit_id IS 'Идентификатор юнита, на который указывает данное выражение'");
      }

      ensureCreatedAt(txn_, schema_, "unit_e_unit");
      ensureLastModifiedAt(txn_, schema_, "unit_e_unit");
    }
  }

  void InitDb::init_undo_tables() const
  {
    const std::string schemaQuoted = pg_.quote_name(schema_);
    {
      if (!hasTable(txn_, schema_, "undo_buffer")) {
        txn_.exec("CREATE TABLE " + schemaQuoted +
                  ".undo_buffer ("
                  "  id          VARCHAR(32)  primary key,"
                  "  target_id   VARCHAR(32)  not null,"
                  "  target_type VARCHAR(150) not null,"
                  "  order_index BIGINT       not null,"
                  "  updated_at  timestamp DEFAULT now()"
                  ")");

        txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".undo_buffer             IS 'Буфер отмены'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_buffer.id          IS 'Идентификатор буфера отмены'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_buffer.target_id   IS 'Ссылается на объект, для которого делается буфер'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_buffer.target_type IS 'Тип цели. Один из: Unit (пока только один)'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_buffer.order_index IS 'Индекс активной операции'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_buffer.updated_at  IS 'Показывает когда что-то изменилось в этом буфере'");
      }

      ensureCreatedAt(txn_, schema_, "undo_buffer");
      ensureLastModifiedAt(txn_, schema_, "undo_buffer");
    }
    {
      if (!hasSequence(txn_, schema_, "undo_op_seq")) {
        txn_.exec("CREATE SEQUENCE " + schemaQuoted + ".undo_op_seq");
        txn_.exec("COMMENT ON SEQUENCE  " + schemaQuoted +
                  ".undo_op_seq IS 'Последовательность для генерации значения"
                  " поля undo_op.order_index'");
      }
      if (!hasTable(txn_, schema_, "undo_op")) {
        txn_.exec("CREATE TABLE " + schemaQuoted +
                  ".undo_op ("
                  "  id               VARCHAR(32)  primary key,"
                  "  undo_buffer_id   VARCHAR(32)  not null,"
                  "  order_index      BIGINT       not null default nextval('" +
                  schemaQuoted +
                  ".undo_op_seq'::regclass),"
                  "  undone           BOOL         not null default false,"
                  "  group_name       TEXT,"
                  "  name             TEXT"
                  ")");

        txn_.exec("COMMENT ON TABLE  " + schemaQuoted + ".undo_op                IS 'Сделанная на данный момент операция'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_op.id             IS 'Идентификатор операции'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_op.undo_buffer_id IS 'Ссылается на буфер, в рамках которого эта операция'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_op.order_index    IS 'Индекс последовательности применения операций'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_op.undone IS 'Признак того, что данная операция отменена. Она доступна для redo'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_op.group_name IS 'Имя группы операций. Отменяется не операции, а группы операций'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_op.name           IS 'Имя операции'");
      }

      ensureCreatedAt(txn_, schema_, "undo_op");
      ensureLastModifiedAt(txn_, schema_, "undo_op");
    }
    {
      if (!hasTable(txn_, schema_, "undo_row_change")) {
        txn_.exec("CREATE TABLE " + schemaQuoted +
                  ".undo_row_change ("
                  "  id               VARCHAR(32)  primary key,"
                  "  undo_op_id       VARCHAR(32)  not null,"
                  "  table_name       TEXT         not null,"
                  "  filter_col_name  TEXT         not null,"
                  "  filter_col_value TEXT         not null,"
                  "  to_delete        BOOL         not null,"
                  "  direction        TEXT CHECK (direction IN ('Forward','ForUndo')),"
                  "  col_name         TEXT,"
                  "  col_value        TEXT"
                  ")");

        txn_.exec("COMMENT ON TABLE  " + schemaQuoted +
                  ".undo_row_change IS '"
                  "Содержит информации об изменении данных в БД.\n"
                  "Применив данную структуру можно изменить одну или несколько строк в таблице.\n"
                  "Изменения происходят такие:\n"
                  "Выбираются строки в таблице `tableName`, у которых колонка `filterColName` имеет значение `filterColValue`.\n"
                  "Если `toDelete == TRUE`, то выбранные строки удаляются.\n"
                  "Если `toDelete == FALSE`, то в выбранных строках значение полей у колонки `colName` принимают значение `value`.'");

        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_row_change.id               IS 'Идентификатор изменения'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                  ".undo_row_change.undo_op_id       IS 'Идентификатор операции отмены (undo_op), которой принадлежит данное изменение'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_row_change.table_name       IS 'Имя таблицы, в которой нужно сделать изменения'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted + ".undo_row_change.filter_col_name  IS 'Имя поля, по которому будет формироваться фильтр'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                  ".undo_row_change.filter_col_value IS '"
                  "Значение для поля фильтра.\n"
                  "Поле фильтра может иметь не текстовый тип - в этом случае нужно конвертировать данный текст в значение этого типа.'");
        txn_.exec(
            "COMMENT ON COLUMN " + schemaQuoted +
            ".undo_row_change.to_delete        IS 'Признак удаления отфильтрованных записей: TRUE - они будут удалены, FALSE - будут изменены'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                  ".undo_row_change.col_name         IS 'Имя колонки, которую нужно изменить, если to_delete == FALSE'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                  ".undo_row_change.col_value        IS '"
                  "Значение, которое нужно присвоить полям в указанной колонке.\n"
                  "Эта колонка может иметь не текстовый тип. В этом случае нужно конвертировать данный текст в этот тип.\n"
                  "Если col_value IS NULL, то нужно присвоить значение NULL.'");
        txn_.exec("COMMENT ON COLUMN " + schemaQuoted +
                  ".undo_row_change.direction IS 'Группа, к которой относиться данное изменение. "
                  "User - Группа изменений, которые сделал пользователь. "
                  "ForUndo - Группа изменений, которые отменяют пользовательские изменений."
                  "Группа ForUndo составляется так, чтобы она полностью отменяла все изменения пользователя'");
      }

      ensureCreatedAt(txn_, schema_, "undo_row_change");
      ensureLastModifiedAt(txn_, schema_, "undo_row_change");
    }
  }

} // namespace back
