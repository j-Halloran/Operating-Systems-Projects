//import linked list for table, and array list for instructions
import java.util.LinkedList;
import java.util.ArrayList; 
import java.util.Hashtable; //hold the pt and future
import java.util.Scanner;
import java.io.*;

/*
* CS 1550 Project 3
* Page Replacement Algorithms
* @author Jake Halloran
* Last Edited: 11/12/16
*/
public class VMSim{
  private static int tau;
  private static int refresh;
  private static int numFrames;
  private static ArrayList<Integer> addressList; //stores address of memory accesses
  private static ArrayList<String> operation; //stores read or write
  private static ArrayList<Integer> fullAddress; //stores not shifted addresses for printing
  private static int numAccesses;
  private static int PAGESIZE = 4096; //page size in bytes
  private static int numWrites;
  private static int numFaults;
  private static Hashtable<Integer, PTENode> pageTable;
  private static Hashtable<Integer, LinkedList<Integer>> future; //holds future references if opt
  private static int[] pageFrames; //array representing frames (Stores pte virtual address)
  private static int workEvictLast; //last used for working set clock evict
  
  public static void main(String[] args){
    //checks inputs before attempting to do anything else
    if(args.length!=5&&args.length!=7&&args.length!=9){
      System.out.print("Usage: Java VMSim -n <numframes> -a <opt|clock|aging|work> [-r <refresh>] [-t <tau>] <tracefile>\n");
      System.exit(0);
    }
    else if(!args[0].equals("-n")){
      System.out.println(args[0]);
      System.out.print("Usage: Java VMSim -n <numframes> -a <opt|clock|aging|work> [-r <refresh>] [-t <tau>] <tracefile>\n");
      System.exit(0);
    }
    
    //passed sanity checks
    //read in input values
    numFrames =  Integer.parseInt(args[1]);
    String traceFile = args[4];
    if(args.length==9){
      refresh = Integer.parseInt(args[5]);
      tau = Integer.parseInt(args[7]);
      traceFile = args[8];
    }
    else if(args.length==7){
      refresh = Integer.parseInt(args[5]);
      traceFile = args[6];
    }
        
    //read in addresses and operations
    Scanner input = null;
    try{
      input= new Scanner(new File(traceFile));
    }
    catch(IOException e){
      System.err.println("Error file not found");
      System.exit(-1);
    }
    
    //initializing ArrayLists
    addressList = new ArrayList<Integer>();
    operation = new ArrayList<String>(1000000);
    fullAddress = new ArrayList<Integer>(1000000);
    future = new Hashtable<Integer, LinkedList<Integer>>();
    pageTable = new Hashtable<Integer, PTENode>();
    
    
    //flag work
    System.out.println("Preprocessing Access List");
     
     
    //optimising if statement 
    boolean optFlag = args[3].equals("opt"); 
    
    //initialize hashtables over 2^20 cycles
    for(int j=0;j<1048576;j++){
      pageTable.put(j,new PTENode());
      if(optFlag){
        future.put(j, new LinkedList<Integer>());
      }
    }
    
    int i =0;
    //read in the addresses and operations
    while(input.hasNext()){
      int pageNumber = Integer.parseUnsignedInt(input.next(),16); //avoids an exception for addresses starting with hex values
      fullAddress.add(pageNumber);
      pageNumber = (pageNumber >>> 12); //only the top 20 bits matter in a 4KB 32bit page table
      addressList.add(pageNumber); 
      operation.add(input.next());
      if(optFlag){
        future.get(pageNumber).add(i); //add to the list of future references to this address
      }
      i++;
    }
    
    //Store number of accesses
    numAccesses = i;
    
    //Initialize the frames
    pageFrames = new int[numFrames];
    for(i = 0;i<numFrames;i++){
      pageFrames[i] = -1;
    }
    
    //call replacement algorithm
    switch(args[3]){
      case "opt": System.out.println("Running optimal replacement algorithm"); opt(); break;
      case "clock": System.out.println("Running clock replacement algorithm"); clock(); break;
      case "aging": System.out.println("Running aging replacement algorithm"); aging(); break;
      case "work": System.out.println("Running working set clock replacement algorithm"); work(); break;
      default: System.out.println("Invalid algorithm choice"); System.exit(-1);
    }
  }
  
  //Optimal replacement algorithm
  private static void opt(){
    //for loop to process all accesses
    for(int i = 0; i<numAccesses;i++){
      int address = addressList.get(i);
      String mode = operation.get(i);
      
      //run replacement with the two tables
      PTENode pte = pageTable.get(address); //get this pte
      future.get(address).removeFirst(); // delete this address from the future
      pte.virtualAddress = address;//set the virtual address of the page
      pte.referenced = true; //mark the page as referenced
      if(mode.equals("W")){
        pte.dirty=true; //mark the page as dirty if writing
      }
      
      //if this page is valid just move on
      if(pte.valid){
        System.out.printf("%x, Hit.\n",fullAddress.get(i));
      }
      else{
        //if fault, count it
        numFaults++;
        
        //for the first few faults, just add them to empty frames
        if((numFaults-1)<numFrames){
          System.out.printf("%x, Page Fault - No Eviction\n",fullAddress.get(i));
          pageFrames[numFaults-1] = address;
          pte.frameNum = numFaults-1;
          pte.valid=true;
        }
        //if we have filled every frame, need to evict something
        else{
          int addrToEvict = getEvictOpt();
          PTENode evictPTE = pageTable.get(addrToEvict);
          
          //If the evicted frame is dirty, write it back
          if(evictPTE.dirty){
            numWrites++;
            System.out.printf("%x, Page Fault - Evict Dirty\n",fullAddress.get(i));
          }
          else{
            //if not writing just say something
            System.out.printf("%x, Page Fault - Evict Clean\n",fullAddress.get(i));
          }
          
          //replace the pte in the frame being evicted from
          pte.frameNum = evictPTE.frameNum;
          pageFrames[pte.frameNum] = address;
          pte.valid = true;
          
          //Mark the evict frame as no longer valid or referenced or dirty or anything else
          pageTable.put(addrToEvict,new PTENode(evictPTE.virtualAddress));
        }
      }
      //store the pte back in the table
      pageTable.put(address,pte);
      
    }
    printResults("Optimal");
  }
  
  //Get address to evict for optimal algorithm
  private static int getEvictOpt(){
    int address = 0; //adddress used furthest in future
    int max = -1; //max time till use found thus far
    for(int i=0;i<numFrames;i++){
      if(future.get(pageFrames[i]).isEmpty()){
        return pageFrames[i]; //if an address is not used again, return that address
      }
      else if(future.get(pageFrames[i]).getFirst()>max){
        //if this future is further than the old max, store the max time and this address
        address = pageFrames[i];
        max = future.get(pageFrames[i]).getFirst();
      }
    }
    
    //return the address matching the furthest in the future use
    return address;
  }
  
  //Clock (second chance) replacement algorithm
  private static void clock(){
    int clockHand = 0;
    for(int i=0;i<numAccesses;i++){
      int address = addressList.get(i);
      String mode = operation.get(i);
      
      //Process the new fault address
      PTENode pte = pageTable.get(address); //get this pte
      pte.virtualAddress = address;//set the virtual address of the page
      pte.referenced = true; //mark the page as referenced
      if(mode.equals("W")){
        pte.dirty=true; //mark the page as dirty if writing
      }
      
      //if this page is valid just move on
      if(pte.valid){
        System.out.printf("%x, Hit.\n",fullAddress.get(i));
      }
      else{
        //if fault, count it
        numFaults++;
        
        //for the first few faults, just add them to empty frames
        if((numFaults-1)<numFrames){
          System.out.printf("%x, Page Fault - No Eviction\n",fullAddress.get(i));
          pageFrames[numFaults-1] = address;
          pte.frameNum = numFaults-1;
          pte.valid=true;
        }
        //if we have filled every frame, need to evict something
        else{
          int addrToEvict = 0; //address to evict foudn by clock
          boolean found = false; //flag to check if we found an evictable frame
          while(!found){
            if(pageTable.get(pageFrames[clockHand]).referenced){ //if the pointed to reference is true
              pageTable.get(pageFrames[clockHand]).referenced=false; //reset the reference
            }
            else{
              addrToEvict = pageFrames[clockHand]; ///save the address
              found = true; //set the flag if found
            }
            clockHand = (clockHand+1)%numFrames; //increment the clock hand
          }
          
          PTENode evictPTE = pageTable.get(addrToEvict);
          
          //If the evicted frame is dirty, write it back
          if(evictPTE.dirty){
            numWrites++;
            System.out.printf("%x, Page Fault - Evict Dirty\n",fullAddress.get(i));
          }
          else{
            //if not writing just say something
            System.out.printf("%x, Page Fault - Evict Clean\n",fullAddress.get(i));
          }
          
          //replace the pte in the frame being evicted from
          pte.frameNum = evictPTE.frameNum;
          pageFrames[pte.frameNum] = address;
          pte.valid = true;
          
          //Mark the evict frame as no longer valid or referenced or dirty or anything else
          pageTable.put(addrToEvict,new PTENode(evictPTE.virtualAddress));
        }
      }
      //store the pte back in the table
      pageTable.put(address,pte);
      
    }
    printResults("Clock");
  }
  
  //Aging replacement algorithm
  private static void aging(){
    int cycleCount = 0;
     //for loop to process all accesses
    for(int i = 0; i<numAccesses;i++){
      //System.out.println(cycleCount);
      //refresh if new cycle count matches refresh
      if(cycleCount==refresh){
        cycleCount = 0;
        agingRefresh();
       // System.exit(0);
      }
      cycleCount++;
      
      int address = addressList.get(i);
      String mode = operation.get(i);
      
      //run replacement with the two tables
      PTENode pte = pageTable.get(address); //get this pte
      pte.virtualAddress = address;//set the virtual address of the page
      pte.referenced = true; //mark the page as referenced
      if(mode.equals("W")){
        pte.dirty=true; //mark the page as dirty if writing
      }
      
      //if this page is valid just move on
      if(pte.valid){
        System.out.printf("%x, Hit.\n",fullAddress.get(i));
      }
      else{
        //if fault, count it
        numFaults++;
        
        //for the first few faults, just add them to empty frames
        if((numFaults-1)<numFrames){
          System.out.printf("%x, Page Fault - No Eviction\n",fullAddress.get(i));
          pageFrames[numFaults-1] = address;
          pte.frameNum = numFaults-1;
          pte.valid=true;
        }
        //if we have filled every frame, need to evict something
        else{
          int addrToEvict = getEvictAging(); 
          PTENode evictPTE = pageTable.get(addrToEvict);
          
          //If the evicted frame is dirty, write it back
          if(evictPTE.dirty){
            numWrites++;
            System.out.printf("%x, Page Fault - Evict Dirty\n",fullAddress.get(i));
          }
          else{
            //if not writing just say something
            System.out.printf("%x, Page Fault - Evict Clean\n",fullAddress.get(i));
          }
          
          //replace the pte in the frame being evicted from
          pte.frameNum = evictPTE.frameNum;
          pageFrames[pte.frameNum] = address;
          pte.valid = true;
          
          //Mark the evict frame as no longer valid or referenced or dirty or anything else
          pageTable.put(addrToEvict,new PTENode(evictPTE.virtualAddress));
        }
      }
      //store the pte back in the table
      pageTable.put(address,pte);
      
    }
    printResults("Aging");
  }
  
  //Refresh method for aging replacement algorithm
  private static void agingRefresh(){
    for(int i = 0;i<numFrames;i++){
      if(pageFrames[i]>-1){
        pageTable.get(pageFrames[i]).incrementAge();
      }
    }
  }
  
  //Find eviction for aging
  private static int getEvictAging(){
    int address = 0; //address to return holder
    int minimum = Integer.MAX_VALUE; //minimum age found
    int temp = 0;
    
    for(int i=0;i <numFrames; i++){
      //takes the current count plus 512*referenced
      if(pageTable.get(pageFrames[i]).referenced) {temp = pageTable.get(pageFrames[i]).agingCount + 512;}
      else{temp = pageTable.get(pageFrames[i]).agingCount;}
      
      if(temp<minimum){
        address = pageFrames[i];
        minimum = temp;
      }
    }
    
    return address;
  }
  
  //working set clock replacement algorithm
  private static void work(){
    int cycleCount = 0;
    workEvictLast = 0;
    //for loop to process all accesses
    for(int i = 0; i<numAccesses;i++){
      
      //refresh if new cycle count matches refresh
      if(cycleCount==refresh){
        cycleCount = 0;
        workRefresh(i);
      }
      cycleCount++;
      
      int address = addressList.get(i);
      String mode = operation.get(i);
      
      //run replacement with the two tables
      PTENode pte = pageTable.get(address); //get this pte
      pte.virtualAddress = address;//set the virtual address of the page
      pte.referenced = true; //mark the page as referenced
      if(mode.equals("W")){
        pte.dirty=true; //mark the page as dirty if writing
      }
      
      //if this page is valid just move on
      if(pte.valid){
        System.out.printf("%x, Hit.\n",fullAddress.get(i));
      }
      else{
        //if fault, count it
        numFaults++;
        
        //for the first few faults, just add them to empty frames
        if((numFaults-1)<numFrames){
          System.out.printf("%x, Page Fault - No Eviction\n",fullAddress.get(i));
          pageFrames[numFaults-1] = address;
          pte.frameNum = numFaults-1;
          pte.valid=true;
        }
        //if we have filled every frame, need to evict something
        else{
          int addrToEvict = getEvictWork(i); 
          PTENode evictPTE = pageTable.get(addrToEvict);
          
          //If the evicted frame is dirty, write it back
          if(evictPTE.dirty){
            numWrites++;
            System.out.printf("%x, Page Fault - Evict Dirty\n",fullAddress.get(i));
          }
          else{
            //if not writing just say something
            System.out.printf("%x, Page Fault - Evict Clean\n",fullAddress.get(i));
          }
          
          //replace the pte in the frame being evicted from
          pte.frameNum = evictPTE.frameNum;
          pageFrames[pte.frameNum] = address;
          pte.valid = true;
          
          //Mark the evict frame as no longer valid or referenced or dirty or anything else
          pageTable.put(addrToEvict,new PTENode(evictPTE.virtualAddress));
        }
      }
      //store the pte back in the table
      pageTable.put(address,pte);
      
    }
    printResults("Working Set Clock");
  }
  
  //refresh for working set clock
  private static void workRefresh(int currentCycle){
    for(int i = 0;i<numFrames;i++){
      if(pageFrames[i]>-1){
        if(pageTable.get(pageFrames[i]).referenced){
          pageTable.get(pageFrames[i]).referenced = false;
          pageTable.get(pageFrames[i]).dereferenceTime = currentCycle;
        }
      }
    }
  }
 
  //find eviction for working set
  private static int getEvictWork(int currentCycle){
    int startPosition = workEvictLast;
    int address = -1; //holder address for oldest
    int oldest = Integer.MAX_VALUE; //holds oldest time
    do{
      if(pageFrames[workEvictLast]>-1){
        //return this address if it is unreferenced and clean
        if(!pageTable.get(pageFrames[workEvictLast]).referenced && !pageTable.get(pageFrames[workEvictLast]).dirty){
          workEvictLast++;
          workEvictLast = workEvictLast % numFrames;
          return pageFrames[workEvictLast];
        }
        //if a page is unreferenced and dirty and older than tau write it back and mark it clean for next pass
        else if(!pageTable.get(pageFrames[workEvictLast]).referenced && pageTable.get(pageFrames[workEvictLast]).dirty){
          if((currentCycle-pageTable.get(pageFrames[workEvictLast]).dereferenceTime)>tau){
            numWrites++;
            pageTable.get(pageFrames[workEvictLast]).dirty = false;
          }
        }
        
        //find the oldest time stap comparator
        if(pageTable.get(pageFrames[workEvictLast]).dereferenceTime < oldest){
          oldest = pageTable.get(pageFrames[workEvictLast]).dereferenceTime;
          address = pageFrames[workEvictLast];
        }
      }
      workEvictLast++;
      workEvictLast = workEvictLast % numFrames;
    }while(workEvictLast!=startPosition);
    
    //if we got here, there were no unreferenced clean pages
    return address;
  }
  
 
  //method to deduplicate results printing
  private static void printResults(String algorithm){
    System.out.println("\nAlgorithm: "+algorithm);
    System.out.println("Number of Frames: "+numFrames);
    System.out.println("Total Memory Accesses: "+numAccesses);
    System.out.println("Total Page Faults: \t"+numFaults);
    System.out.println("Total Writes to Disk: \t"+ numWrites);
  }
}