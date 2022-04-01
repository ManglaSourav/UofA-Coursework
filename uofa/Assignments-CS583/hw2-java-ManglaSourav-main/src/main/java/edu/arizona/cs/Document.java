package edu.arizona.cs;

public class Document {
     String docid;
     int position1;
     int position2;

    public Document(String docid_in, int position1_in,int position2_in){
        docid=docid_in;
        position1=position1_in;
        position2=position2_in;
    }

    public boolean equals(Document answer) {
        if  (!docid.equals(answer.docid))
        {
            return false;
        }
        if  (position1!=answer.position1)
        {
            return false;
        }
        if  (position2!=answer.position2)
        {
            return false;
        }
    return true;
    }
}
