
extern void print(int i);
extern int get_int();

class rectangle
{
public:
    rectangle(int x, int y, int height, int width);

    void set_x(int x);
    void set_y(int y);
    void set_height(int h);
    void set_width(int w);

    int get_x() const;
    int get_y() const;
    int get_height() const;
    int get_width() const;

    void compute_area();

    void print() const;

public:
    int m_area;
private:
    int m_x;
    int m_y;
    int m_height;
    int m_width;
};

rectangle::rectangle(int x, int y, int height, int width)
    : m_x(x)
    , m_y(y)
    , m_height(height)
    , m_width(width)
{
}

void rectangle::set_x(int x)
{
    m_x = x;
}

void rectangle::set_y(int y)
{
    m_y = y;
}

void rectangle::set_height(int h)
{
    m_height = h;
}

void rectangle::set_width(int w)
{
    m_width = w;
}

int rectangle::get_x() const
{
    return m_x;
}

int rectangle::get_y() const
{
    return m_y;
}

int rectangle::get_height() const
{
    return m_height;
}

int rectangle::get_width() const
{
    return m_width;
}

void rectangle::compute_area()
{
    m_area = m_width * m_height;
}

void rectangle::print() const
{
    ::print(m_x);
    ::print(m_y);
    ::print(m_width);
    ::print(m_height);
    ::print(m_area);
}

int main()
{
    int x = 3;
    int y = -1;
    int height = 21;
    int width = 12;
    rectangle rect(x, y, height, width);

    int w = rect.get_width();

    //rect.compute_area();
    int area = rect.m_area;
    //rect.print(); // all should be input indep

    rect.set_x(get_int());
    //area = rect.m_area; // still input indep
    rect.compute_area();
    //area = rect.m_area; // input dep
    //print(area);

    x = rect.get_x();
    y = rect.get_y();
    //print(x);
    //print(y);

    return 0;
}

