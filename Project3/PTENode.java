//page table entry node for cs1550 project 3
//everything is public cause im lazy
public class PTENode{
  public boolean valid;
  public boolean dirty;
  public boolean referenced;
  public int virtualAddress;
  public int frameNum;
  public char agingCount;
  public int dereferenceTime;
  
  public PTENode(){
    this.valid = false;
    this.dirty = false;
    this.referenced = false;
    this.virtualAddress = 0;
    this.frameNum = -1;
    this.agingCount = 0;
    this.dereferenceTime = 0;
  }
  
  //second constructor to preserve virtual address
  public PTENode(int address){
    this.valid = false;
    this.dirty = false;
    this.referenced = false;
    this.virtualAddress = address;
    this.frameNum = -1;
    this.agingCount = 0;
    this.dereferenceTime = 0;
  }
  
  public void incrementAge(){
    if(referenced){
      agingCount = (char)(agingCount >>> 1);
      agingCount = (char)(agingCount | (1 << 8));
    }
    else{
      agingCount = (char)(agingCount >>> 1);
    }
  }
}