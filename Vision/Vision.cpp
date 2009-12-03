/*!
  @file Vision.h
  @brief Declaration of NUbots Vision class.
  @author Steven Nicklin
*/
#include "Vision.h"
#include "Tools/Image/NUImage.h"
#include "ClassificationColours.h"
#include <QDebug>

Vision::Vision()
{
    return;
}

Vision::~Vision()
{
    return;
}

unsigned char Vision::classifyPixel(int x, int y)
{
    pixels::Pixel* temp = &currentImage->image[y][x];
    return currentLookupTable[(temp->y<<16) + (temp->cb<<8) + temp->cr];
}

void Vision::classifyImage(ClassifiedImage &target, const NUimage* sourceImage, const unsigned char *lookUpTable)
{   
    target.setImageDimensions(sourceImage->width(),sourceImage->height());
    currentImage = sourceImage;
    currentLookupTable = lookUpTable;
    for (int y = 0; y < sourceImage->height(); y++)
    {
        for (int x = 0; x < sourceImage->width(); x++)
        {
            target.image[y][x] = classifyPixel(x,y);
        }
    }
    return;
}

std::vector< Vector2<int> > Vision::findGreenBorderPoints(const NUimage* sourceImage, const unsigned char *lookUpTable, int scanSpacing, Horizon* horizonLine)
{
    std::vector< Vector2<int> > results;
    currentImage = sourceImage;
    currentLookupTable = lookUpTable;
    int yStart;
    int consecutiveGreenPixels = 0;
    for (int x = 0; x < sourceImage->width(); x+=scanSpacing)
    {
        yStart = (int)horizonLine->findYFromX(x);
        if(yStart > sourceImage->height()) continue;
        if(yStart < 0) yStart = 0;
        consecutiveGreenPixels = 0;
        for (int y = yStart; y < sourceImage->height(); y++)
        {
            if(classifyPixel(x,y) == ClassIndex::green)
            {
                consecutiveGreenPixels++;
            }
            else
            {
                consecutiveGreenPixels = 0;
            }
            if(consecutiveGreenPixels >= 3)
            {
                results.push_back(Vector2<int>(x,y-consecutiveGreenPixels+1));
                break;
            }
        }
    }
    return results;
}

#define LEFT_OF(x0, x1, x2) ((x1.x-x0.x)*(-x2.y+x0.y)-(x2.x-x0.x)*(-x1.y+x0.y) > 0)

std::vector<Vector2<int> > Vision::getConvexFieldBorders(std::vector<Vector2<int> >& fieldBorders)
{
  //Andrew's Monotone Chain Algorithm to compute the upper hull
  std::vector<Vector2<int> > hull;
  if(!fieldBorders.size()) return hull;
  const std::vector<Vector2<int> >::const_iterator pmin = fieldBorders.begin(),
                                                   pmax = fieldBorders.end()-1;
  hull.push_back(*pmin);
  for(std::vector<Vector2<int> >::const_iterator pi = pmin + 1; pi != pmax+1; pi++)
  {
    if(!LEFT_OF((*pmin), (*pmax), (*pi)) && pi != pmax)
      continue;

    while((int)hull.size() > 1)
    {
      const std::vector<Vector2<int> >::const_iterator p1 = hull.end() - 1,
                                                       p2 = hull.end() - 2;
      if(LEFT_OF((*p1), (*p2), (*pi)))
        break;
      hull.pop_back();
    }
    hull.push_back(*pi);
  }
  return hull;
}

std::vector<Vector2<int> > Vision::interpolateBorders(std::vector<Vector2<int> >& fieldBorders, int scanSpacing)
{
    std::vector<Vector2<int> > interpolatedBorders;
    if(!fieldBorders.size()) return interpolatedBorders;
    std::vector<Vector2<int> >::const_iterator nextPoint = fieldBorders.begin();
    std::vector<Vector2<int> >::const_iterator prevPoint = nextPoint++;

    int x = prevPoint->x;
    Vector2<int> deltaPoint, temp;
    for (; nextPoint != fieldBorders.end(); nextPoint++)
    {
        deltaPoint = (*nextPoint) - (*prevPoint);
        for (; x <= nextPoint->x; x+=scanSpacing)
        {
            temp.x = x;
            temp.y = (x - prevPoint->x) * deltaPoint.y / deltaPoint.x + prevPoint->y;
            if (temp.y < 0) temp.y = 0;
            if (temp.y >= currentImage->height()) temp.y = currentImage->height() - 1;
            interpolatedBorders.push_back(temp);
        }
        prevPoint = nextPoint;
    }
    return interpolatedBorders;
}



std::vector<Vector2<int> > Vision::verticalScan(std::vector<Vector2<int> >&fieldBorders,int scanSpacing)
{
    std::vector<Vector2<int> > scanPoints;
    if(!fieldBorders.size()) return scanPoints;
    std::vector<Vector2<int> >::const_iterator nextPoint = fieldBorders.begin();
    std::vector<Vector2<int> >::const_iterator prevPoint = nextPoint++;
    int x = 0;
    int y = 0;
    int halfLineEnd = 0;
    int quarterLineEnd = 0;
    int midX = 0;
    int skip = int(scanSpacing/2);

    Vector2<int> temp;
    for (; nextPoint != fieldBorders.end(); nextPoint++)
    {
        x = nextPoint->x;
        y = nextPoint->y;
        halfLineEnd = y + int((currentImage->height() - y)/2);
        quarterLineEnd = y+ int((currentImage->height() - y)/8);
        //qDebug() << y << ','<< halfLineEnd;
        midX = x-skip;
        //qDebug() << midX << ", "<<x;
        //unsigned char colour;
        while(y < currentImage->height())
        {
            //colour = classifyPixel(x,y);
            //qDebug() << x << ',' << y << ':' << (int)colour;
            temp.x = x;
            temp.y = y;
            scanPoints.push_back(temp);
            if(y < halfLineEnd)
            {
                temp.x = midX;

                temp.y = y;
                scanPoints.push_back(temp);

                if(y < quarterLineEnd)
                {
                    temp.x = midX-skip/2;
                    temp.y = y;
                    scanPoints.push_back(temp);
                    temp.x = midX+skip/2;
                    temp.y = y;
                    scanPoints.push_back(temp);
                }
            }
            y++;
        }
    }
    //LAST LINE

    midX = fieldBorders.back().x+skip;
    y = fieldBorders.back().y;
    while(y < currentImage->height())
    {
        if(y < halfLineEnd)
        {
            temp.x = midX;
            temp.y = y;
            scanPoints.push_back(temp);
            if(y < quarterLineEnd)
            {
                temp.x = midX-skip/2;
                temp.y = y;
                scanPoints.push_back(temp);
                temp.x = midX+skip/2;
                temp.y = y;
                scanPoints.push_back(temp);
            }
        }
        y++;

    }

    return scanPoints;
}

std::vector<Vector2<int> > Vision::horizontalScan(std::vector<Vector2<int> >&fieldBorders,int scanSpacing)
{
    std::vector<Vector2<int> > horizontalScanPoints;
    Vector2<int> temp;
    if(!currentImage) return horizontalScanPoints;
    if(!fieldBorders.size())
    {
        for(int y = 0; y < currentImage->height(); y = y + scanSpacing*2)
        {
            for(int x = 0; x < currentImage->width(); x++)
            {
                temp.x = x;
                temp.y = y;
                //qDebug()<< "adding";
                horizontalScanPoints.push_back(temp);
            }
        }
        //qDebug()<< "returning";
        return horizontalScanPoints;
    }

    //Find the minimum Y, and scan above the field boarders
    std::vector<Vector2<int> >::const_iterator nextPoint = fieldBorders.begin();
    std::vector<Vector2<int> >::const_iterator prevPoint = nextPoint++;
    int minY = currentImage->height();
    int maxY = 0;
    for (; nextPoint != fieldBorders.end(); nextPoint++)
    {
        if(nextPoint->y < minY)
        {
            minY = nextPoint->y;
        }
        if(nextPoint->y >maxY)
        {
            maxY = nextPoint->y;
        }
    }
    //Then calculate horizontal scanlines above the field boarder

    for(int y = minY; y > 0; y = y - scanSpacing)
    {
        for(int x = 0; x < currentImage->width(); x++)
        {
            temp.x = x;
            temp.y = y;
            horizontalScanPoints.push_back(temp);
        }
    }
    for(int y = minY; y < maxY; y = y + scanSpacing*2)
    {
        for(int x = 0; x < currentImage->width(); x++)
        {
            temp.x = x;
            temp.y = y;
            horizontalScanPoints.push_back(temp);
        }
    }
    return horizontalScanPoints;
}